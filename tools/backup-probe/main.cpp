#include "sbcoop/save/backup_verifier.hpp"

#include <filesystem>
#include <iostream>

int wmain(int count, wchar_t** args) {
    if (count != 4) {
        std::cerr << "Usage: sbcoop_backup_probe <worker-script> <backup-directory> <expected-archive-SHA256>\n";
        return 2;
    }
    const auto hash = std::filesystem::path(args[3]).string();
    sbcoop::save::BackupVerifier verifier({std::filesystem::absolute(args[1]), std::filesystem::absolute(args[2]), hash});
    const auto result = verifier.verify();
    std::cout << "check_id=" << result.check.check_id << " state=" << static_cast<int>(result.check.state)
              << " failure=" << static_cast<int>(result.failure) << " hash=" << result.check.archive_sha256 << '\n';
    return result.failure == sbcoop::save::BackupFailure::none ? 0 : 1;
}
