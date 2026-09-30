#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "sbcoop/save/backup_verifier.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace sbcoop::save {
namespace {
class Handle {
public:
    explicit Handle(HANDLE value = nullptr) : value_(value) {}
    ~Handle() { if (value_ && value_ != INVALID_HANDLE_VALUE) { CloseHandle(value_); } }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return value_; }
    explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
private:
    HANDLE value_;
};
// Windows argv quoting; no cmd.exe or PowerShell expression evaluates a path.
std::wstring quote(std::wstring_view argument) {
    std::wstring output{L"\""};
    std::size_t slashes = 0;
    for (const auto c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        output.append(slashes * (c == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (c == L'"') { output += L'\\'; }
        output += c;
    }
    output.append(slashes * 2, L'\\');
    return output + L'"';
}
bool valid_path_text(const std::filesystem::path& path) {
    const auto text = path.native();
    return path.is_absolute() && !text.empty() && text.size() < 4096 &&
        std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L' ' && c != L'"'; });
}
bool no_links(const std::filesystem::path& path) {
    for (auto current = path; !current.empty(); current = current.parent_path()) {
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { return false; }
        if (current == current.parent_path()) { break; }
    }
    return true;
}
class Attributes {
public:
    bool initialize() {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        storage_.resize(size);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        initialized_ = InitializeProcThreadAttributeList(list_, 1, 0, &size) != FALSE;
        return initialized_;
    }
    ~Attributes() { if (initialized_) { DeleteProcThreadAttributeList(list_); } }
    LPPROC_THREAD_ATTRIBUTE_LIST get() const { return list_; }
private:
    std::vector<unsigned char> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_{};
    bool initialized_{};
};
}

BackupVerification BackupVerifier::verify() {
    const std::lock_guard lock(mutex_);
    BackupVerification result{{BackupState::unknown, {}, 0}, BackupFailure::invalid_config};
    if (next_check_ == std::numeric_limits<std::uint64_t>::max()) { return result; }
    result.check.check_id = ++next_check_;
    if (!valid_path_text(config_.script) || !valid_path_text(config_.backup_directory) ||
        config_.timeout < std::chrono::milliseconds{1} || config_.timeout > std::chrono::minutes{1} ||
        config_.expected_archive_sha256.size() != 64 ||
        !std::all_of(config_.expected_archive_sha256.begin(), config_.expected_archive_sha256.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        }) || !no_links(config_.script)) { return result; }
    std::array<wchar_t, MAX_PATH> windows{};
    const auto length = GetSystemWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
    if (length == 0 || length >= windows.size()) { return result; }
    const auto powershell = std::filesystem::path(windows.data()) / "System32/WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command = quote(powershell.native()) + L" -NoLogo -NoProfile -NonInteractive -File " +
        quote(config_.script.native()) + L" -BackupDirectory " + quote(config_.backup_directory.native()) +
        L" -ExpectedArchiveSha256 " + quote(std::wstring(config_.expected_archive_sha256.begin(), config_.expected_archive_sha256.end()));
    result.failure = BackupFailure::launch_failed;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE reader_raw{}, writer_raw{};
    if (!CreatePipe(&reader_raw, &writer_raw, &security, 0)) { return result; }
    Handle reader(reader_raw), writer(writer_raw);
    if (!SetHandleInformation(reader.get(), HANDLE_FLAG_INHERIT, 0)) { return result; }
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr));
    if (!input) { return result; }
    Attributes attributes;
    if (!attributes.initialize()) { return result; }
    HANDLE inherited[]{writer.get(), input.get()};
    if (!UpdateProcThreadAttribute(attributes.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  inherited, sizeof(inherited), nullptr, nullptr)) { return result; }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.get();
    startup.StartupInfo.hStdOutput = writer.get();
    startup.StartupInfo.hStdError = writer.get();
    startup.lpAttributeList = attributes.get();
    Handle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) { return result; }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { return result; }
    PROCESS_INFORMATION process_info{};
    if (!CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
        config_.script.parent_path().c_str(), &startup.StartupInfo, &process_info)) { return result; }
    Handle process(process_info.hProcess), thread(process_info.hThread);
    if (!AssignProcessToJobObject(job.get(), process.get()) || ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 1000);
        return result;
    }
    const auto deadline = std::chrono::steady_clock::now() + config_.timeout;
    std::string output;
    bool done = false;
    auto read_output = [&]() {
        DWORD available{};
        while (PeekNamedPipe(reader.get(), nullptr, 0, nullptr, &available, nullptr) && available) {
            if (available > 128 - output.size()) { return false; }
            std::array<char, 128> bytes{};
            DWORD read{};
            if (!ReadFile(reader.get(), bytes.data(), available, &read, nullptr) || read == 0) { return false; }
            output.append(bytes.data(), read);
        }
        return true;
    };
    while (!done) {
        if (!read_output()) {
            TerminateJobObject(job.get(), 1);
            result.failure = BackupFailure::invalid_result;
            return result;
        }
        const auto waited = WaitForSingleObject(process.get(), 10);
        if (waited == WAIT_OBJECT_0) { done = true; }
        else if (waited == WAIT_FAILED) { return result; }
        else if (std::chrono::steady_clock::now() >= deadline) {
            TerminateJobObject(job.get(), 1);
            WaitForSingleObject(process.get(), 1000);
            result.failure = BackupFailure::timeout;
            return result;
        }
    }
    DWORD code{};
    if (!read_output() || !GetExitCodeProcess(process.get(), &code)) {
        result.failure = BackupFailure::invalid_result;
        return result;
    }
    return decode_backup_result(output, code, config_.expected_archive_sha256, result.check.check_id);
}
} // namespace sbcoop::save
