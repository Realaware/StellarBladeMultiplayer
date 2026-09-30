#include "sbcoop/save/save_system.hpp"

#include <algorithm>
#include <exception>

namespace sbcoop::save {
namespace {
bool valid_text(std::string_view value) {
    return !value.empty() && value.size() <= 256 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 0x20 && c != 0x7f; });
}
}
bool valid_backup_check(const BackupCheck& backup) {
    return backup.state == BackupState::verified && backup.check_id != 0 &&
           backup.archive_sha256.size() == 64 &&
           std::all_of(backup.archive_sha256.begin(), backup.archive_sha256.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
           });
}
namespace {
bool same_slot(const Identity& a, const Identity& b) { return a.account == b.account && a.slot == b.slot; }
}
bool valid_observation(const Observation& observation) {
    if (observation.generation == 0 || !valid_text(observation.account) || !valid_text(observation.profile) ||
        observation.saves.size() > 64) { return false; }
    for (std::size_t i = 0; i < observation.saves.size(); ++i) {
        const auto& save = observation.saves[i];
        if (!valid_identity(save) || save.account != observation.account || save.profile != observation.profile) { return false; }
        for (std::size_t j = 0; j < i; ++j) { if (same_slot(save, observation.saves[j])) { return false; } }
    }
    if (observation.state == LoadedState::loaded) {
        if (!observation.loaded || !valid_identity(*observation.loaded) ||
            observation.loaded->account != observation.account || observation.loaded->profile != observation.profile) { return false; }
        if (observation.inventory_complete &&
            std::find(observation.saves.begin(), observation.saves.end(), *observation.loaded) == observation.saves.end()) { return false; }
    } else if (observation.loaded) { return false; }
    return true;
}
bool valid_identity(const Identity& identity) {
    return valid_text(identity.account) && valid_text(identity.slot) &&
           valid_text(identity.instance) && valid_text(identity.profile);
}
bool valid_id(std::string_view id) {
    return id.size() == 32 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string_view describe(Reason reason) {
    switch (reason) {
    case Reason::allowed: return "Multiplayer save verified";
    case Reason::backup_unverified: return "Verify the retained save backup";
    case Reason::native_binding_unverified: return "Game save integration is unavailable";
    case Reason::loaded_unknown: return "Loaded save could not be identified";
    case Reason::loading: return "Waiting for the save to finish loading";
    case Reason::no_loaded_save: return "Load a multiplayer save";
    case Reason::identity_invalid: return "Save identity is invalid or inconsistent";
    case Reason::unflagged: return "This save was not created for multiplayer";
    case Reason::ambiguous_flag: return "Conflicting multiplayer save records";
    case Reason::flag_store_error: return "Multiplayer save records could not be read or written";
    case Reason::busy: return "A multiplayer save is already being created";
    case Reason::unsafe_creation_context: return "Return to the supported new-save screen";
    case Reason::inventory_unknown: return "Available save slots could not be verified";
    case Reason::invalid_transaction: return "Save creation request is invalid or expired";
    case Reason::creation_failed: return "Save creation was not confirmed; multiplayer remains blocked";
    case Reason::occupied_slot: return "Save creation would affect an existing slot";
    case Reason::stale_observation: return "Save changed; check the loaded save again";
    }
    return "Save status is unknown";
}

Decision SaveSystem::check_loaded(const Observation& observation, const BackupCheck& backup) const {
    Decision result;
    result.generation = observation.generation;
    if (!valid_backup_check(backup)) { result.reason = Reason::backup_unverified; return result; }
    if (!observation.read_binding_verified) { result.reason = Reason::native_binding_unverified; return result; }
    if (!valid_observation(observation)) { result.reason = Reason::identity_invalid; return result; }
    if (observation.state != LoadedState::loaded) {
        result.reason = observation.state == LoadedState::loading ? Reason::loading :
                        observation.state == LoadedState::none ? Reason::no_loaded_save : Reason::loaded_unknown;
        return result;
    }
    try {
        const auto flags = store_.read_all();
        if (flags.size() > 64) { result.reason = Reason::flag_store_error; return result; }
        for (const auto& flag : flags) {
            if (!valid_id(flag.coop_id) || !valid_identity(flag.identity)) { result.reason = Reason::flag_store_error; return result; }
            if (flag.identity == *observation.loaded) {
                if (result.flag) { result.flag.reset(); result.reason = Reason::ambiguous_flag; return result; }
                result.flag = flag;
            }
        }
    } catch (const std::exception&) { result.reason = Reason::flag_store_error; return result; }
    result.reason = result.flag ? Reason::allowed : Reason::unflagged;
    return result;
}

Reason SaveSystem::creation_readiness(const Observation& observation, const BackupCheck& backup) const {
    if (pending_) { return Reason::busy; }
    if (!valid_backup_check(backup) || backup.check_id <= last_creation_backup_check_) { return Reason::backup_unverified; }
    if (!observation.read_binding_verified || !observation.creation_binding_verified) { return Reason::native_binding_unverified; }
    if (!valid_observation(observation)) { return Reason::identity_invalid; }
    if (!observation.inventory_complete) { return Reason::inventory_unknown; }
    if (!observation.creation_context_safe || observation.state != LoadedState::none) { return Reason::unsafe_creation_context; }
    return Reason::allowed;
}

Reason SaveSystem::begin_creation(const Observation& observation, const BackupCheck& backup, std::string transaction_id) {
    const auto readiness = creation_readiness(observation, backup);
    if (readiness != Reason::allowed) { return readiness; }
    if (!valid_id(transaction_id)) { return Reason::invalid_transaction; }
    CreationRequest request{std::move(transaction_id), observation.generation,
                            observation.account, observation.profile, observation.saves};
    try {
        if (!store_.write_intent(request)) { return Reason::flag_store_error; }
    } catch (const std::exception&) { return Reason::flag_store_error; }
    last_creation_backup_check_ = backup.check_id;
    pending_ = std::move(request);
    return Reason::allowed;
}

Reason SaveSystem::finish_creation(const CreationReceipt& receipt, const Observation& after) {
    if (!pending_ || receipt.transaction_id != pending_->transaction_id) { return Reason::invalid_transaction; }
    // Failures retain the intent as recovery evidence. They never produce a flag.
    auto reject = [this](Reason reason) { pending_.reset(); return reason; };
    if (!receipt.native_creation_verified || !receipt.persistence_confirmed) { return reject(Reason::creation_failed); }
    if (!valid_identity(receipt.created) || receipt.created.account != pending_->account ||
        receipt.created.profile != pending_->profile) { return reject(Reason::identity_invalid); }
    for (const auto& old : pending_->occupied_saves) {
        if (same_slot(old, receipt.created) || old.instance == receipt.created.instance) { return reject(Reason::occupied_slot); }
    }
    if (!after.read_binding_verified || !valid_observation(after) || !after.inventory_complete ||
        after.state != LoadedState::loaded || after.loaded != receipt.created ||
        after.generation <= pending_->observed_generation) { return reject(Reason::stale_observation); }
    // Creation must retain the whole previously observed native inventory.
    if (after.saves.size() != pending_->occupied_saves.size() + 1) { return reject(Reason::creation_failed); }
    for (const auto& old : pending_->occupied_saves) {
        if (std::find(after.saves.begin(), after.saves.end(), old) == after.saves.end()) { return reject(Reason::creation_failed); }
    }
    const Flag flag{pending_->transaction_id, receipt.created};
    try {
        if (!store_.commit_created(flag)) { return reject(Reason::flag_store_error); }
    } catch (const std::exception&) { return reject(Reason::flag_store_error); }
    pending_.reset();
    return Reason::allowed;
}

void SaveSystem::fail_creation() { pending_.reset(); }
bool SaveSystem::permit_matches(const Decision& permit, const Observation& current) {
    return permit.allowed() && permit.flag && current.read_binding_verified && valid_observation(current) &&
           current.state == LoadedState::loaded && current.generation == permit.generation &&
           current.loaded == permit.flag->identity;
}

} // namespace sbcoop::save
