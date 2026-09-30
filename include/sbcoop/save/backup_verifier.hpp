#pragma once

#include "sbcoop/save/save_system.hpp"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <utility>

namespace sbcoop::save {

enum class BackupFailure { none, invalid_config, launch_failed, timeout, invalid_result, missing, corrupt };
struct BackupVerification {
    BackupCheck check;
    BackupFailure failure{BackupFailure::invalid_result};
};

// Fixed local worker protocol. No save paths, native objects or script text are
// returned to the coordinator. A VERIFIED response also requires exit code 0.
BackupVerification decode_backup_result(std::string_view output, unsigned long exit_code,
                                        std::string_view expected_hash, std::uint64_t check_id);

struct BackupVerifierConfig {
    std::filesystem::path script;
    std::filesystem::path backup_directory;
    std::string expected_archive_sha256;
    std::chrono::milliseconds timeout{30000};
};

// Windows implementation launches the project verifier through -File, without
// a shell or command expression. Call on a filesystem worker, never a game/UI
// callback. Operations are serialized and allocate fresh nonzero check IDs.
class BackupVerifier {
public:
    explicit BackupVerifier(BackupVerifierConfig config) : config_(std::move(config)) {}
    BackupVerification verify();
private:
    BackupVerifierConfig config_;
    std::mutex mutex_;
    std::uint64_t next_check_{};
};

} // namespace sbcoop::save
