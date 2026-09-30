#include "sbcoop/save/runtime_controller.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <stdexcept>

using namespace sbcoop::save;
using namespace std::chrono_literals;
namespace {
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
struct Store final : IFlagStore {
    std::vector<Flag> flags;
    std::vector<CreationRequest> intents;
    std::thread::id owner{};
    void check() const { require(owner == std::this_thread::get_id(), "store used off filesystem worker"); }
    std::vector<Flag> read_all() const override { check(); return flags; }
    bool write_intent(const CreationRequest& request) override { check(); intents.push_back(request); return true; }
    bool commit_created(const Flag& flag) override { check(); flags.push_back(flag); return true; }
};
struct Gate {
    std::mutex mutex;
    std::condition_variable ready;
    bool entered{}, released{};
    void wait() {
        std::unique_lock lock(mutex); entered = true; ready.notify_all();
        require(ready.wait_for(lock, 3s, [this] { return released; }), "test gate was not released");
    }
    void await_entry() {
        std::unique_lock lock(mutex);
        require(ready.wait_for(lock, 3s, [this] { return entered; }), "worker did not reach gate");
    }
    void release() { const std::lock_guard lock(mutex); released = true; ready.notify_all(); }
    ~Gate() { release(); }
};
const Identity solo{"account", "solo", "solo-instance", "profile"};
const Identity coop{"account", "coop", "coop-instance", "profile"};
const std::string transaction(32, 'a');
Observation menu(std::uint64_t generation = 1) {
    return {generation, true, true, true, true, solo.account, solo.profile, LoadedState::none, std::nullopt, {solo}};
}
Observation loaded_coop(std::uint64_t generation = 3) {
    auto value = menu(generation); value.state = LoadedState::loaded; value.loaded = coop; value.saves.push_back(coop); return value;
}
template<class Predicate> RuntimeSnapshot await_state(RuntimeController& runtime, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = runtime.snapshot();
        if (predicate(state)) { return state; }
        std::this_thread::sleep_for(1ms);
    }
    const auto state = runtime.snapshot();
    throw std::runtime_error("runtime did not publish expected state: " + std::string(describe(state.reason)) +
        ", generation=" + std::to_string(state.observation_generation) + ", request=" + std::to_string(state.request_id) +
        ", checking=" + std::to_string(state.checking) + ", busy=" + std::to_string(state.busy) +
        ", failure=" + std::to_string(static_cast<int>(state.failure)));
}
template<class Predicate> void await_condition(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) { return; }
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("runtime condition timed out");
}
void lifecycle() {
    Store store;
    std::uint64_t check{};
    const auto caller = std::this_thread::get_id();
    RuntimeController runtime(store, [&] {
        store.owner = std::this_thread::get_id();
        require(store.owner != caller, "backup verification blocked the caller thread");
        return BackupCheck{BackupState::verified, std::string(64, 'A'), ++check};
    }, [] { return transaction; }, 7);
    require(runtime.observe(6, menu()) == SubmitResult::stale_epoch, "old runtime epoch accepted");
    require(runtime.observe(7, menu()) == SubmitResult::accepted, "menu rejected");
    await_state(runtime, [](const auto& s) { return s.creation_enabled; });
    require(runtime.submit(7, RuntimeAction::create, 1) == SubmitResult::accepted, "create command rejected");
    require(!runtime.snapshot().decision.allowed(), "permission survived a new UI request");
    std::optional<CreationRequest> request;
    await_condition([&] { request = runtime.pop_creation(); return request.has_value(); });
    require(request->transaction_id == transaction && request->occupied_saves == std::vector<Identity>{solo}, "creation request lost inventory");
    auto loading = menu(2); loading.state = LoadedState::loading;
    require(runtime.observe(7, loading) == SubmitResult::accepted, "loading observation rejected");
    require(runtime.complete(7, {transaction, true, true, coop}, loaded_coop()) == SubmitResult::accepted, "receipt rejected");
    await_state(runtime, [](const auto& s) { return s.decision.allowed(); });
    require(runtime.permit_matches(loaded_coop()), "flagged loaded save denied");
    auto replaced = loaded_coop(4); replaced.loaded->instance = "replaced"; replaced.saves.back() = *replaced.loaded;
    runtime.observe(7, replaced);
    require(!runtime.permit_matches(replaced), "permission survived changed instance");
    await_state(runtime, [](const auto& s) { return !s.checking && s.reason == Reason::unflagged; });
    runtime.close();
    require(store.flags.size() == 1 && store.intents.size() == 1, "wrong number of durable operations");
    require(runtime.snapshot().failure == RuntimeFailure::closed && !runtime.permit_matches(loaded_coop()), "closed runtime granted access");
}
void overflow() {
    Store store;
    Gate gate;
    RuntimeController runtime(store, [&] { store.owner = std::this_thread::get_id(); gate.wait(); return BackupCheck{BackupState::verified, std::string(64, 'A'), 1}; },
        [] { return transaction; }, 9, 1);
    runtime.observe(9, menu()); gate.await_entry();
    require(runtime.submit(9, RuntimeAction::refresh, 1) == SubmitResult::accepted, "bounded queue refused first item");
    require(runtime.submit(9, RuntimeAction::create, 1) == SubmitResult::overflow, "overflow not explicit");
    require(runtime.snapshot().failure == RuntimeFailure::overflow && !runtime.pop_creation(), "overflow allowed native dispatch");
    gate.release(); runtime.close();
    require(store.intents.empty() && store.flags.empty(), "overflow created metadata");
}
void corrupt_at_completion() {
    Store store;
    std::uint64_t check{};
    RuntimeController runtime(store, [&] {
        store.owner = std::this_thread::get_id(); ++check;
        return BackupCheck{check < 3 ? BackupState::verified : BackupState::corrupt, std::string(64, 'A'), check};
    }, [] { return transaction; }, 2);
    runtime.observe(2, menu()); await_state(runtime, [](const auto& s) { return s.creation_enabled; });
    runtime.submit(2, RuntimeAction::create, 1);
    std::optional<CreationRequest> request;
    await_condition([&] { request = runtime.pop_creation(); return request.has_value(); });
    runtime.complete(2, {transaction, true, true, coop}, loaded_coop(2));
    await_state(runtime, [](const auto& s) { return !s.checking && !s.busy && s.reason == Reason::backup_unverified; });
    runtime.close(); require(store.flags.empty() && store.intents.size() == 1, "corrupt backup committed flag");
}
void stale_generation_and_backup() {
    Store store;
    RuntimeController runtime(store, [&] { store.owner = std::this_thread::get_id(); return BackupCheck{BackupState::verified, std::string(64, 'A'), 1}; },
        [] { return transaction; }, 3);
    runtime.observe(3, menu(2)); await_state(runtime, [](const auto& s) { return s.creation_enabled; });
    require(runtime.observe(3, menu(1)) == SubmitResult::stale_generation, "old observation replaced latest");
    runtime.submit(3, RuntimeAction::create, 1);
    await_state(runtime, [](const auto& s) { return !s.checking && s.reason == Reason::stale_observation; });
    runtime.submit(3, RuntimeAction::create, 2);
    await_state(runtime, [](const auto& s) { return !s.checking && s.reason == Reason::backup_unverified; });
    auto contradictory = menu(2); contradictory.creation_context_safe = false;
    require(runtime.observe(3, contradictory) == SubmitResult::invalid_observation, "generation reuse accepted changed context");
    runtime.close(); require(store.intents.empty() && store.flags.empty(), "stale request created metadata");
}
void cancelled_dispatch_and_retry() {
    Store store;
    std::uint64_t check{};
    char id = 'a';
    RuntimeController runtime(store, [&] {
        store.owner = std::this_thread::get_id();
        return BackupCheck{BackupState::verified, std::string(64, 'A'), ++check};
    }, [&] { return std::string(32, id++); }, 5);
    runtime.observe(5, menu()); await_state(runtime, [](const auto& s) { return s.creation_enabled; });
    runtime.submit(5, RuntimeAction::create, 1);
    await_state(runtime, [](const auto& s) { return !s.checking && s.busy; });
    runtime.creation_failed(5, transaction);
    require(!runtime.pop_creation(), "cancelled request dispatched before cancellation was processed");
    await_state(runtime, [](const auto& s) { return !s.checking && !s.busy && s.reason == Reason::creation_failed; });
    require(!runtime.pop_creation(), "cancelled request remained in outbound queue");
    runtime.submit(5, RuntimeAction::create, 1);
    std::optional<CreationRequest> request;
    await_condition([&] { request = runtime.pop_creation(); return request.has_value(); });
    require(request->transaction_id == std::string(32, 'b'), "retry returned an older creation request");
    runtime.creation_failed(5, request->transaction_id);
    await_state(runtime, [](const auto& s) { return !s.checking && !s.busy; });
    runtime.close(); require(store.flags.empty() && store.intents.size() == 2, "cancel/retry lost intent evidence or wrote a flag");
}
void receipt_without_dispatch() {
    Store store;
    std::uint64_t check{};
    RuntimeController runtime(store, [&] {
        store.owner = std::this_thread::get_id();
        return BackupCheck{BackupState::verified, std::string(64, 'A'), ++check};
    }, [] { return transaction; }, 6);
    runtime.observe(6, menu()); await_state(runtime, [](const auto& s) { return s.creation_enabled; });
    runtime.submit(6, RuntimeAction::create, 1);
    await_state(runtime, [](const auto& s) { return !s.checking && s.busy; });
    runtime.complete(6, {transaction, true, true, coop}, loaded_coop(2));
    await_state(runtime, [](const auto& s) { return !s.busy; });
    require(!runtime.pop_creation(), "undispatched receipt left a native request queued");
    runtime.close(); require(store.flags.empty() && store.intents.size() == 1, "undispatched receipt committed a flag");
}
void worker_exception() {
    Store store;
    RuntimeController runtime(store, []() -> BackupCheck { throw std::runtime_error("verifier failed"); },
        [] { return transaction; }, 8);
    runtime.observe(8, menu());
    await_state(runtime, [](const auto& s) { return s.failure == RuntimeFailure::worker_exception; });
    require(!runtime.pop_creation() && !runtime.permit_matches(loaded_coop()), "worker exception granted native work or permission");
    runtime.close(); require(store.flags.empty() && store.intents.empty(), "worker exception wrote metadata");
}
}
int main() {
    try {
        std::cout << "lifecycle\n"; lifecycle();
        std::cout << "overflow\n"; overflow();
        std::cout << "corrupt_at_completion\n"; corrupt_at_completion();
        std::cout << "stale_generation_and_backup\n"; stale_generation_and_backup();
        std::cout << "cancelled_dispatch_and_retry\n"; cancelled_dispatch_and_retry();
        std::cout << "receipt_without_dispatch\n"; receipt_without_dispatch();
        std::cout << "worker_exception\n"; worker_exception();
        std::cout << "Save worker lifecycle, queue faults, backup freshness and generation tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
