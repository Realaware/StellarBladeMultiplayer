#include "sbcoop/save/backup_verifier.hpp"

#include <algorithm>

namespace sbcoop::save {
namespace {
bool valid_hash(std::string_view hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    });
}
}
BackupVerification decode_backup_result(std::string_view output, unsigned long exit_code,
                                        std::string_view expected_hash, std::uint64_t check_id) {
    BackupVerification result{{BackupState::unknown, {}, check_id}, BackupFailure::invalid_result};
    if (check_id == 0 || !valid_hash(expected_hash) || output.size() > 128) { return result; }
    if (output.ends_with("\r\n")) { output.remove_suffix(2); }
    else if (output.ends_with("\n")) { output.remove_suffix(1); }
    else { return result; }
    constexpr std::string_view prefix = "SBCOOP_BACKUP/1 VERIFIED ";
    if (exit_code == 0 && output.starts_with(prefix) && output.substr(prefix.size()) == expected_hash) {
        return {{BackupState::verified, std::string(expected_hash), check_id}, BackupFailure::none};
    }
    if (exit_code == 3 && output == "SBCOOP_BACKUP/1 MISSING") {
        return {{BackupState::missing, {}, check_id}, BackupFailure::missing};
    }
    if (exit_code == 4 && output == "SBCOOP_BACKUP/1 CORRUPT") {
        return {{BackupState::corrupt, {}, check_id}, BackupFailure::corrupt};
    }
    return result;
}
} // namespace sbcoop::save
