#include "sbcoop/save/file_flag_store.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sbcoop::save {
namespace {
constexpr std::size_t max_file_size = 80'000;
void require_plain_path(const std::filesystem::path& path) {
    auto current = path;
    while (!current.empty()) {
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            throw std::runtime_error("linked metadata paths are unsupported");
        }
#else
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(current))) {
            throw std::runtime_error("linked metadata paths are unsupported");
        }
#endif
        const auto parent = current.parent_path();
        if (parent == current) { break; }
        current = parent;
    }
}
void append_string(std::string& data, std::string_view value) {
    if (value.empty() || value.size() > 256 || value.find_first_of("\r\n\0", 0, 3) != std::string_view::npos) {
        throw std::runtime_error("invalid metadata string");
    }
    data += std::to_string(value.size()) + ':';
    data.append(value);
    data += '\n';
}
std::string read_string(std::string_view data, std::size_t& cursor) {
    const auto colon = data.find(':', cursor);
    if (colon == std::string_view::npos || colon == cursor || colon - cursor > 3) { throw std::runtime_error("bad field size"); }
    std::size_t count = 0;
    for (auto i = cursor; i < colon; ++i) {
        if (data[i] < '0' || data[i] > '9') { throw std::runtime_error("bad field size"); }
        count = count * 10 + static_cast<std::size_t>(data[i] - '0');
    }
    if (count == 0 || count > 256 || colon + 1 + count >= data.size() || data[colon + 1 + count] != '\n') {
        throw std::runtime_error("truncated metadata field");
    }
    auto result = std::string(data.substr(colon + 1, count));
    cursor = colon + 2 + count;
    return result;
}
void append_identity(std::string& data, const Identity& identity) {
    if (!valid_identity(identity)) { throw std::runtime_error("invalid save identity"); }
    append_string(data, identity.account); append_string(data, identity.slot);
    append_string(data, identity.instance); append_string(data, identity.profile);
}
Identity read_identity(std::string_view data, std::size_t& cursor) {
    Identity result{read_string(data, cursor), read_string(data, cursor), read_string(data, cursor), read_string(data, cursor)};
    if (!valid_identity(result)) { throw std::runtime_error("invalid save identity"); }
    return result;
}
std::string flag_data(const Flag& flag) {
    if (!valid_id(flag.coop_id)) { throw std::runtime_error("invalid co-op ID"); }
    std::string data = "SBCOOP-FLAG-1\n";
    append_string(data, flag.coop_id); append_identity(data, flag.identity);
    return data;
}
std::string read_file(const std::filesystem::path& path) {
    require_plain_path(path);
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > max_file_size) { throw std::runtime_error("metadata size limit"); }
    std::ifstream input(path, std::ios::binary);
    if (!input) { throw std::runtime_error("cannot open metadata"); }
    std::string result(static_cast<std::size_t>(size), '\0');
    input.read(result.data(), static_cast<std::streamsize>(result.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) { throw std::runtime_error("metadata changed or truncated"); }
    return result;
}
// Exclusive creation, flush, then publication without replacing an old record.
bool write_new(const std::filesystem::path& final_path, std::string_view data) {
    require_plain_path(final_path);
    const auto partial = std::filesystem::path(final_path.native() + std::filesystem::path(".partial").native());
#ifdef _WIN32
    const auto handle = CreateFileW(partial.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { return false; }
    DWORD written = 0;
    const bool copied = WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0 && written == data.size();
    const bool flushed = copied && FlushFileBuffers(handle) != 0;
    CloseHandle(handle);
    if (!flushed) { return false; }
    return MoveFileExW(partial.c_str(), final_path.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    const auto descriptor = open(partial.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor == -1) { return false; }
    const bool copied = write(descriptor, data.data(), data.size()) == static_cast<ssize_t>(data.size());
    const bool flushed = copied && fsync(descriptor) == 0;
    close(descriptor);
    if (!flushed || link(partial.c_str(), final_path.c_str()) != 0) { return false; }
    // No automatic pruning; the linked partial also remains as evidence.
    return true;
#endif
}
Flag parse_flag(std::string_view data) {
    constexpr std::string_view header = "SBCOOP-FLAG-1\n";
    if (!data.starts_with(header)) { throw std::runtime_error("unsupported flag format"); }
    auto cursor = header.size();
    Flag flag{read_string(data, cursor), read_identity(data, cursor)};
    if (!valid_id(flag.coop_id) || cursor != data.size()) { throw std::runtime_error("invalid flag record"); }
    return flag;
}
}

FileFlagStore::FileFlagStore(std::filesystem::path directory) : directory_(std::filesystem::absolute(std::move(directory)).lexically_normal()) {
    require_plain_path(directory_);
    std::filesystem::create_directories(directory_);
}
std::vector<Flag> FileFlagStore::read_all() const {
    require_plain_path(directory_);
    std::vector<Flag> flags;
    std::size_t entries = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (++entries > 256) { throw std::runtime_error("metadata directory limit"); }
        require_plain_path(entry.path());
        if (entry.path().extension() != ".flag") { continue; }
        if (!entry.is_regular_file() || flags.size() >= 64) { throw std::runtime_error("flag count/type limit"); }
        auto flag = parse_flag(read_file(entry.path()));
        if (entry.path().stem().string() != flag.coop_id) { throw std::runtime_error("flag filename mismatch"); }
        flags.push_back(std::move(flag));
    }
    return flags;
}
bool FileFlagStore::write_intent(const CreationRequest& request) {
    if (!valid_id(request.transaction_id) || request.observed_generation == 0 || request.occupied_saves.size() > 64) { return false; }
    std::string data = "SBCOOP-INTENT-1\n";
    append_string(data, request.transaction_id); append_string(data, request.account); append_string(data, request.profile);
    append_string(data, std::to_string(request.observed_generation));
    append_string(data, std::to_string(request.occupied_saves.size()));
    for (const auto& identity : request.occupied_saves) { append_identity(data, identity); }
    return write_new(directory_ / (request.transaction_id + ".intent"), data);
}
bool FileFlagStore::commit_created(const Flag& flag) {
    if (!valid_id(flag.coop_id) || !valid_identity(flag.identity)) { return false; }
    const auto intent_path = directory_ / (flag.coop_id + ".intent");
    require_plain_path(intent_path);
    if (!std::filesystem::exists(intent_path)) { return false; }
    const auto intent = read_file(intent_path);
    constexpr std::string_view header = "SBCOOP-INTENT-1\n";
    if (!intent.starts_with(header)) { return false; }
    auto cursor = header.size();
    if (read_string(intent, cursor) != flag.coop_id || read_string(intent, cursor) != flag.identity.account ||
        read_string(intent, cursor) != flag.identity.profile) { return false; }
    // Native creation validation is exclusively the SaveSystem's responsibility.
    return write_new(directory_ / (flag.coop_id + ".flag"), flag_data(flag));
}

} // namespace sbcoop::save
