#include "sbcoop/save/file_flag_store.hpp"
#include "sbcoop/save/save_system.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sbcoop::save;
namespace {
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
struct MemoryStore final : IFlagStore {
    std::vector<Flag> flags;
    std::vector<CreationRequest> intents;
    bool fail_read{}, fail_write{};
    std::vector<Flag> read_all() const override { if (fail_read) { throw std::runtime_error("unreadable"); } return flags; }
    bool write_intent(const CreationRequest& request) override { if (fail_write) { return false; } intents.push_back(request); return true; }
    bool commit_created(const Flag& flag) override { if (fail_write) { return false; } flags.push_back(flag); return true; }
};
const Identity solo{"test-account", "solo", "solo-instance", "test-build"};
const Identity coop{"test-account", "coop", "coop-instance", "test-build"};
const std::string id(32, 'a');
BackupCheck good_backup(std::uint64_t check = 1) { return {BackupState::verified, std::string(64, 'A'), check}; }
Observation menu() {
    return {1, true, true, true, true, solo.account, solo.profile, LoadedState::none, std::nullopt, {solo}};
}
Observation loaded(const Identity& identity, std::uint64_t generation = 2) {
    auto observation = menu();
    observation.generation = generation;
    observation.state = LoadedState::loaded;
    observation.loaded = identity;
    if (identity != solo) { observation.saves.push_back(identity); }
    return observation;
}
void test_creation_and_admission() {
    MemoryStore store;
    SaveSystem system(store);
    require(system.check_loaded(loaded(solo), good_backup()).reason == Reason::unflagged, "solo admitted");
    require(system.begin_creation(menu(), {}, id) == Reason::backup_unverified, "backup gate missing");
    auto unknown = menu(); unknown.creation_binding_verified = false;
    require(system.begin_creation(unknown, good_backup(), id) == Reason::native_binding_unverified, "unverified native create admitted");
    require(system.begin_creation(loaded(solo), good_backup(), id) == Reason::unsafe_creation_context, "creation allowed in solo gameplay");
    require(system.begin_creation(menu(), good_backup(), id) == Reason::allowed, "create start failed");
    require(store.flags.empty() && store.intents.size() == 1, "flag written before creation");
    require(system.begin_creation(menu(), good_backup(2), std::string(32, 'b')) == Reason::busy, "concurrent create allowed");
    require(system.finish_creation({std::string(32, 'c'), true, true, coop}, loaded(coop)) == Reason::invalid_transaction,
            "wrong transaction accepted");
    require(system.busy(), "unrelated receipt cancelled pending creation");
    require(system.finish_creation({id, true, true, coop}, loaded(coop)) == Reason::allowed, "creation receipt failed");
    require(store.flags.size() == 1 && !system.busy(), "flag not committed");
    const auto permit = system.check_loaded(loaded(coop), good_backup(2));
    require(permit.allowed(), "created save blocked");
    require(SaveSystem::permit_matches(permit, loaded(coop)), "matching permit rejected");
    require(!SaveSystem::permit_matches(permit, loaded(coop, 3)), "permit survived reload");
    require(!SaveSystem::permit_matches(permit, loaded(solo)), "permit survived switching to solo");
    auto replaced = coop; replaced.instance = "replacement-solo-instance";
    require(system.check_loaded(loaded(replaced), good_backup(2)).reason == Reason::unflagged, "reused slot inherited flag");
    require(system.check_loaded(loaded(coop), {BackupState::corrupt, {}, 3}).reason == Reason::backup_unverified, "corrupt backup admitted");
    auto loading = menu(); loading.state = LoadedState::loading;
    require(system.check_loaded(loading, good_backup()).reason == Reason::loading, "loading admitted");
    auto unreadable = menu(); unreadable.state = LoadedState::unknown;
    require(system.check_loaded(unreadable, good_backup()).reason == Reason::loaded_unknown, "unknown admitted");
    store.flags.push_back({std::string(32, 'b'), coop});
    require(system.check_loaded(loaded(coop), good_backup()).reason == Reason::ambiguous_flag, "duplicate flags admitted");
    store.fail_read = true;
    require(system.check_loaded(loaded(coop), good_backup()).reason == Reason::flag_store_error, "store failure admitted");
    store.fail_read = false; store.flags.clear();
    require(system.check_loaded(loaded(coop), good_backup()).reason == Reason::unflagged, "missing flag admitted");
    require(system.begin_creation(menu(), good_backup(), std::string(32, 'd')) == Reason::backup_unverified, "old creation backup check reused");
}
void test_failed_creation() {
    for (int failure = 0; failure < 6; ++failure) {
        MemoryStore store; SaveSystem system(store);
        require(system.begin_creation(menu(), good_backup(), id) == Reason::allowed, "failure fixture start");
        CreationReceipt receipt{id, true, true, coop}; auto after = loaded(coop);
        if (failure == 0) { receipt.persistence_confirmed = false; }
        if (failure == 1) { receipt.created = solo; after = loaded(solo); }
        if (failure == 2) { after.generation = 1; }
        if (failure == 3) { after.saves.erase(after.saves.begin()); }
        if (failure == 4) { after.loaded = solo; }
        if (failure == 5) { store.fail_write = true; }
        require(system.finish_creation(receipt, after) != Reason::allowed, "unsafe creation committed");
        require(store.flags.empty(), "failed creation got flag");
        require(!system.busy(), "failed creation remained busy");
    }
    MemoryStore store; SaveSystem system(store);
    auto observation = menu(); observation.saves.push_back(solo);
    require(system.begin_creation(observation, good_backup(), id) == Reason::identity_invalid, "duplicate native slots accepted");
    observation = menu(); observation.inventory_complete = false;
    require(system.begin_creation(observation, good_backup(), id) == Reason::inventory_unknown, "unknown inventory accepted");
}
void test_durable_store() {
    const auto directory = std::filesystem::current_path() / "save-store-fixtures" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    FileFlagStore store(directory); SaveSystem system(store);
    require(!store.commit_created({id, coop}), "orphan commit succeeded");
    require(system.begin_creation(menu(), good_backup(), id) == Reason::allowed, "durable intent failed");
    FileFlagStore interrupted(directory);
    require(interrupted.read_all().empty(), "interrupted intent counted as a flag");
    require(!interrupted.write_intent(*system.pending_request()), "existing intent overwritten");
    require(system.finish_creation({id, true, true, coop}, loaded(coop)) == Reason::allowed, "durable commit failed");
    FileFlagStore restarted(directory); SaveSystem resumed(restarted);
    require(resumed.check_loaded(loaded(coop), good_backup()).allowed(), "flag did not survive restart");
    require(!restarted.commit_created({id, solo}), "old flag overwritten");
    require(resumed.check_loaded(loaded(coop), good_backup()).allowed(), "overwrite attempt changed old flag");
    // A truncated published record blocks admission rather than treating it as solo.
    std::ofstream corrupt(directory / (id + ".flag"), std::ios::binary | std::ios::trunc);
    corrupt << "SBCOOP-FLAG-1\n"; corrupt.close();
    require(resumed.check_loaded(loaded(coop), good_backup()).reason == Reason::flag_store_error, "truncated flag admitted");
    std::cout << "Fixture metadata retained: " << directory.string() << '\n';
}
}
int main() {
    try {
        test_creation_and_admission(); test_failed_creation(); test_durable_store();
        std::cout << "PASS: save creation, admission, identity replacement, generations, failures and durable flags\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
