#include "sbcoop/save/backup_verifier.hpp"

#include <iostream>
#include <stdexcept>

using namespace sbcoop::save;
namespace {
void require(bool value, const char* message) { if (!value) { throw std::runtime_error(message); } }
}
int main() {
    try {
        const std::string hash(64, 'A');
        const auto wire = "SBCOOP_BACKUP/1 VERIFIED " + hash + "\r\n";
        const auto valid = decode_backup_result(wire, 0, hash, 19);
        require(valid.failure == BackupFailure::none && valid.check.state == BackupState::verified &&
                valid.check.check_id == 19 && valid.check.archive_sha256 == hash, "verified response lost identity");
        require(decode_backup_result("SBCOOP_BACKUP/1 MISSING\n", 3, hash, 20).check.state == BackupState::missing, "missing not classified");
        require(decode_backup_result("SBCOOP_BACKUP/1 CORRUPT\n", 4, hash, 20).check.state == BackupState::corrupt, "corrupt not classified");
        require(decode_backup_result(wire, 1, hash, 19).check.state == BackupState::unknown, "failed process accepted");
        require(decode_backup_result(wire, 0, std::string(64, 'B'), 19).check.state == BackupState::unknown, "foreign archive accepted");
        require(decode_backup_result(wire, 0, hash, 0).check.state == BackupState::unknown, "zero check ID accepted");
        require(decode_backup_result(wire + "\n", 0, hash, 19).check.state == BackupState::unknown, "trailing output accepted");
        require(decode_backup_result("noise\n" + wire, 0, hash, 19).check.state == BackupState::unknown, "leading output accepted");
        for (std::size_t i = 0; i < wire.size() - 2; ++i) {
            require(decode_backup_result(wire.substr(0, i), 0, hash, 19).check.state == BackupState::unknown, "truncated response accepted");
        }
        auto wrong_version = wire; wrong_version[14] = '2';
        require(decode_backup_result(wrong_version, 0, hash, 19).check.state == BackupState::unknown, "unknown version accepted");
        require(decode_backup_result(std::string(129, 'X'), 0, hash, 19).check.state == BackupState::unknown, "oversized output accepted");
        require(decode_backup_result("SBCOOP_BACKUP/1 MISSING\n", 0, hash, 19).check.state == BackupState::unknown, "inconsistent exit accepted");
        std::cout << "Backup result identity/failure tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
