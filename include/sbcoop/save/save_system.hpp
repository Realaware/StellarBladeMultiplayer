#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sbcoop::save {

// Project value contracts, not native game types. An instance ID must be
// persistent through autosaves/reloads and change when a slot is recreated.
struct Identity {
    std::string account;
    std::string slot;
    std::string instance;
    std::string profile;
    bool operator==(const Identity&) const = default;
};

bool valid_identity(const Identity& identity);
bool valid_id(std::string_view id);

enum class LoadedState { unknown, none, loading, loaded };
struct Observation {
    std::uint64_t generation{};
    bool read_binding_verified{};
    bool creation_binding_verified{};
    bool creation_context_safe{};
    bool inventory_complete{};
    std::string account;
    std::string profile;
    LoadedState state{LoadedState::unknown};
    std::optional<Identity> loaded;
    std::vector<Identity> saves;
    bool operator==(const Observation&) const = default;
};
bool valid_observation(const Observation& observation);

enum class BackupState { unknown, missing, corrupt, verified };
struct BackupCheck {
    BackupState state{BackupState::unknown};
    std::string archive_sha256;
    std::uint64_t check_id{};
};
bool valid_backup_check(const BackupCheck& backup);

struct Flag {
    std::string coop_id; // 32 lowercase hexadecimal characters
    Identity identity;
    bool operator==(const Flag&) const = default;
};

struct CreationRequest {
    std::string transaction_id;
    std::uint64_t observed_generation{};
    std::string account;
    std::string profile;
    std::vector<Identity> occupied_saves;
};

// Only a verified adapter can produce a receipt. The adapter must independently
// confirm native creation, successful persistence, and preservation of old saves.
struct CreationReceipt {
    std::string transaction_id;
    bool native_creation_verified{};
    bool persistence_confirmed{};
    Identity created;
};

class IFlagStore {
public:
    virtual ~IFlagStore() = default;
    virtual std::vector<Flag> read_all() const = 0;
    virtual bool write_intent(const CreationRequest& request) = 0;
    virtual bool commit_created(const Flag& flag) = 0;
};

enum class Reason {
    allowed, backup_unverified, native_binding_unverified, loaded_unknown,
    loading, no_loaded_save, identity_invalid, unflagged, ambiguous_flag,
    flag_store_error, busy, unsafe_creation_context, inventory_unknown,
    invalid_transaction, creation_failed, occupied_slot, stale_observation
};
std::string_view describe(Reason reason);

struct Decision {
    Reason reason{Reason::native_binding_unverified};
    std::optional<Flag> flag;
    std::uint64_t generation{};
    bool allowed() const { return reason == Reason::allowed; }
};

// Consumes immutable observations captured by the game adapter. It never calls
// native code. Its caller must serialize operations and use a fresh backup check
// for each creation/admission; UI code cannot supply native observations.
class SaveSystem {
public:
    explicit SaveSystem(IFlagStore& store) : store_(store) {}
    Decision check_loaded(const Observation& observation, const BackupCheck& backup) const;
    Reason creation_readiness(const Observation& observation, const BackupCheck& backup) const;
    Reason begin_creation(const Observation& observation, const BackupCheck& backup,
                          std::string transaction_id);
    std::optional<CreationRequest> pending_request() const { return pending_; }
    Reason finish_creation(const CreationReceipt& receipt, const Observation& after);
    void fail_creation();
    bool busy() const { return pending_.has_value(); }
    // Admission tokens are invalid across any native loading/generation change.
    static bool permit_matches(const Decision& permit, const Observation& current);

private:
    IFlagStore& store_;
    std::optional<CreationRequest> pending_;
    std::uint64_t last_creation_backup_check_{};
};

} // namespace sbcoop::save
