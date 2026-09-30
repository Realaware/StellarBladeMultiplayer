#include "sbcoop/save/backup_verifier.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sbcoop::save;
namespace {
void require(bool value, const char* message) { if (!value) { throw std::runtime_error(message); } }
}
int wmain(int count, wchar_t** args) {
    try {
        require(count == 5, "worker tests need script, fixture backup, hash and script-fixture directory");
        const auto script = std::filesystem::absolute(args[1]);
        const auto backup = std::filesystem::absolute(args[2]);
        const auto hash = std::filesystem::path(args[3]).string();
        const auto fixture = std::filesystem::absolute(args[4]);
        BackupVerifier verifier({script, backup, hash});
        const auto first = verifier.verify();
        const auto second = verifier.verify();
        require(first.failure == BackupFailure::none && second.failure == BackupFailure::none &&
            first.check.state == BackupState::verified && first.check.archive_sha256 == hash &&
            second.check.check_id > first.check.check_id, "real archive not verified with fresh ID");
        BackupVerifier missing({script, fixture / "absent", hash});
        require(missing.verify().check.state == BackupState::missing, "missing archive admitted");
        const auto foreign_hash = hash == std::string(64, 'A') ? std::string(64, 'B') : std::string(64, 'A');
        BackupVerifier foreign({script, backup, foreign_hash});
        require(foreign.verify().check.state == BackupState::corrupt, "foreign retained identity admitted");
        BackupVerifier slow({fixture / "slow.ps1", backup, hash, std::chrono::milliseconds{1500}});
        const auto started = std::chrono::steady_clock::now();
        require(slow.verify().failure == BackupFailure::timeout, "timeout failed to block");
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds{5}, "worker deadline not enforced");
        BackupVerifier noisy({fixture / "noise.ps1", backup, hash});
        require(noisy.verify().failure == BackupFailure::invalid_result, "oversized child output admitted");
        BackupVerifier invalid({script, backup, "not-a-hash"});
        require(invalid.verify().failure == BackupFailure::invalid_config, "invalid config launched");
        // Corruption touches only this disposable fixture, never a user archive.
        std::ofstream archive(backup / "SaveGames.zip", std::ios::binary | std::ios::app);
        archive.put('X'); archive.close();
        require(verifier.verify().check.state == BackupState::corrupt, "corruption after verification reused stale success");
        require(!std::filesystem::exists(fixture / "sentinel"), "path expression was executed");
        std::cout << "Real backup worker, identity, timeout, quoting and corruption tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
