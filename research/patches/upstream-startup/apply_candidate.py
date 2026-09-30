"""Apply the reviewed startup patch only to a separate, exact-pinned source copy.

No downloads, game/process access, installation, or native save operation.
The source/output manifest remains ignored build evidence, not game qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path

PINNED = {
    "UE4SS/include/UE4SSProgram.hpp": "AFDEA0F67204EE1FFCDAED0C3A02A444029222754C5981D50A46D538BFAD98BC",
    "UE4SS/src/UE4SSProgram.cpp": "5841A44FB1782C161447B383A6F36B75696F04111CCFB44143F65C8DE8292092",
    "UE4SS/src/main_ue4ss_rewritten.cpp": "79FBFCFBC69B7E2AA42500E246937E6C8B60C041071C3C4B0A683EF7D3F2395E",
    "deps/first/Unreal/include/Unreal/UnrealInitializer.hpp": "CA08DDE374687E0C035299FA7AD43AF0C6BF481DAF0F1053AE8E6F8FBD2C7787",
    "deps/first/Unreal/src/UnrealInitializer.cpp": "5CB97A31ACC657D0218555F66FE095869242B53698D5B58FA89694CB1E1CBA65",
    "UE4SS/CMakeLists.txt": "978ABE8441FD365C57191B99DA8B5D0B7FEBB11FEBD2C47CEF77C2F97A4C1A65",
}


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def replace(text, before, after, count=1):
    if text.count(before) != count:
        raise ValueError(f"Patch context count differed: {before[:100]!r}")
    return text.replace(before, after)


def program_header(text):
    text = replace(text, "#include <mutex>", "#include <mutex>\n#include <memory>\n#include <Unreal/StartupGuard.hpp>")
    text = replace(text, "        static inline std::atomic_bool cpp_mods_done_loading{};", """        // Legacy bootstrap release only; this flag does not establish native readiness.
        static inline std::atomic_bool cpp_mods_done_loading{};
        static inline std::atomic<Startup::BootstrapState> bootstrap_state{Startup::BootstrapState::pending};
        std::shared_ptr<Startup::Guard> m_startup_guard;
        auto fail_startup(const char* message) -> void;""")
    text = replace(text, "UE4SSProgram(const std::filesystem::path& ModuleFilePath, std::initializer_list<BinaryOptions> options);", "UE4SSProgram(const std::filesystem::path& ModuleFilePath, std::initializer_list<BinaryOptions> options, Startup::Clock::time_point startup_started = Startup::Clock::now());")
    return text


def program_source(text):
    text = replace(text, "UE4SSProgram::UE4SSProgram(const std::filesystem::path& moduleFilePath, std::initializer_list<BinaryOptions> options) : MProgram(options)", "UE4SSProgram::UE4SSProgram(const std::filesystem::path& moduleFilePath, std::initializer_list<BinaryOptions> options, Startup::Clock::time_point startup_started)\n        : MProgram(options), m_startup_guard(std::make_shared<Startup::Guard>(120, startup_started))")
    text = replace(text, "                create_emergency_console_for_early_error(fmt::format(STR(\"The IniParser failed to parse: {}\"), ensure_str(e.what())));", "                fail_startup(e.what());\n                create_emergency_console_for_early_error(fmt::format(STR(\"The IniParser failed to parse: {}\"), ensure_str(e.what())));")
    text = replace(text, "            if (settings_manager.CrashDump.EnableDumping)", """            // Charge constructor work to the same deadline as native initialization.
            m_startup_guard = std::make_shared<Startup::Guard>(settings_manager.General.SecondsToScanBeforeGivingUp, startup_started);
            m_startup_guard->check_phase("program configuration");

            if (settings_manager.CrashDump.EnableDumping)""")
    text = replace(text, "            setup_mods();\n            install_cpp_mods();\n            start_cpp_mods(IsInitialStartup::Yes);", """            m_startup_guard->check_phase("C++ mod setup");
            setup_mods();
            m_startup_guard->check_phase("C++ mod installation");
            install_cpp_mods();
            m_startup_guard->check_phase("C++ mod startup");
            start_cpp_mods(IsInitialStartup::Yes);
            m_startup_guard->check_phase("C++ mod startup completion");""")
    old_catch = """        catch (std::runtime_error& e)
        {
            // Returns to main from here which checks, displays & handles whether to close the program or not
            // If has_error() returns false that means that set_error was not called
            // In that case we need to copy the exception message to the error buffer before we return to main
            if (!m_error_object->has_error())
            {
                copy_error_into_message(e.what());
            }
            return;
        }"""
    text = replace(text, old_catch, """        catch (const std::exception& e)
        {
            fail_startup(e.what());
            return;
        }
        catch (...)
        {
            fail_startup("Unknown exception during UE4SS startup");
            return;
        }""", 2)
    text = replace(text, "    auto UE4SSProgram::init() -> void", """    auto UE4SSProgram::fail_startup(const char* message) -> void
    {
        m_startup_guard->fail();
        Unreal::UnrealInitializer::IsInitialized().store(false, std::memory_order_release);
        bootstrap_state.store(Startup::BootstrapState::failed, std::memory_order_release);
        // Retain program/callback state. Failure does not authorize explicit unload.
        cpp_mods_done_loading.store(true, std::memory_order_release);
        cpp_mods_done_loading.notify_all();
        if (!m_error_object->has_error())
        {
            copy_error_into_message(message);
        }
    }

    auto UE4SSProgram::init() -> void""")
    text = replace(text, "            setup_unreal();", """            m_startup_guard->check_phase("native initialization entry");
            if (m_error_object->has_error())
            {
                throw std::runtime_error("Program construction failed before native initialization");
            }
            setup_unreal();
            m_startup_guard->check_phase("native initialization completion");""")
    text = replace(text, "            fire_unreal_init_for_cpp_mods();\n            setup_unreal_properties();", "            m_startup_guard->check_phase(\"C++ Unreal initialization callbacks\");\n            fire_unreal_init_for_cpp_mods();\n            m_startup_guard->check_phase(\"Unreal property setup\");\n            setup_unreal_properties();\n            m_startup_guard->check_phase(\"asset registry configuration\");")
    text = replace(text, "            share_lua_functions();", "            m_startup_guard->check_phase(\"shared function publication\");\n            share_lua_functions();\n            m_startup_guard->check_phase(\"shared function publication completion\");\n#ifdef RUN_TESTS\n            m_startup_guard->mark_native_ready();\n#endif")
    text = replace(text, "            m_event_loop = std::jthread{&UE4SSProgram::update, this};", """            m_startup_guard->check_phase("event thread creation");
            m_event_loop = std::jthread{[this] {
                try { update(); }
                catch (const std::exception& e) { fail_startup(e.what()); }
                catch (...) { fail_startup("Unhandled UE4SS update-thread exception"); }
            }};""")
    text = replace(text, "        on_program_start();", "        m_startup_guard->check_phase(\"program-start callbacks\");\n        on_program_start();\n        m_startup_guard->mark_native_ready();")
    text = replace(text, "        TRY([&] {\n            ObjectDumper::init();", "        [&] {\n            ObjectDumper::init();")
    text = replace(text, "            start_lua_mods();\n        });", "            start_lua_mods();\n        }();")
    text = replace(text, "        config.SecondsToScanBeforeGivingUp = settings_manager.General.SecondsToScanBeforeGivingUp;", "        config.SecondsToScanBeforeGivingUp = settings_manager.General.SecondsToScanBeforeGivingUp;\n        config.StartupGuard = m_startup_guard;")
    text = replace(text, "                // No work to be done here. Error is non-fatal, just let the 'Initialize' function take it from here.", "                // An ordinary failed pre-scan may retry after bootstrap release.\n                // A terminal deadline/failure must never be swallowed.\n                m_startup_guard->check_phase(\"early scan retry\");")
    text = replace(text, "        cpp_mods_done_loading.store(true);\n        cpp_mods_done_loading.notify_one();", "        m_startup_guard->mark_bootstrap_complete();\n        auto expected_bootstrap = Startup::BootstrapState::pending;\n        bootstrap_state.compare_exchange_strong(expected_bootstrap, Startup::BootstrapState::released, std::memory_order_acq_rel);\n        cpp_mods_done_loading.store(true, std::memory_order_release);\n        cpp_mods_done_loading.notify_all();")
    return text


def entry_source(text):
    text = replace(text, "auto thread_dll_start(UE4SSProgram* program) -> unsigned long\n{", "auto WINAPI thread_dll_start(void* context) -> DWORD\n{\n    auto* program = static_cast<UE4SSProgram*>(context);")
    text = replace(text, "    program->init();", """    try
    {
        program->init();
    }
    catch (const std::exception& e)
    {
        program->fail_startup(e.what());
    }
    catch (...)
    {
        program->fail_startup("Unhandled startup-thread exception");
    }""")
    text = replace(text, "    if (auto e = program->get_error_object(); e->has_error())", "    try\n    {\n    if (auto e = program->get_error_object(); e->has_error())")
    text = replace(text, "\n    return 0;\n}", "\n    }\n    catch (...) { OutputDebugStringW(L\"UE4SS startup error reporting failed\\n\"); }\n    return 0;\n}")
    old = """    auto program = new UE4SSProgram(moduleFilenameBuffer, {});
    if (HANDLE handle = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(thread_dll_start), (LPVOID)program, 0, nullptr); handle)
    {
        CloseHandle(handle);
    }

    if (s_wait_for_ue4ss)
    {
        UE4SSProgram::cpp_mods_done_loading.wait(false, std::memory_order_relaxed);
    }"""
    text = replace(text, old, """    const auto startup_started = Startup::Clock::now();
    // Retained hooks/threads require mapped code. This research candidate cannot
    // be explicitly unloaded; normal process closure reclaims its resources.
    HMODULE pinned_module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&thread_dll_start), &pinned_module))
    {
        UE4SSProgram::bootstrap_state.store(Startup::BootstrapState::failed, std::memory_order_release);
        UE4SSProgram::cpp_mods_done_loading.store(true, std::memory_order_release);
        UE4SSProgram::cpp_mods_done_loading.notify_all();
        OutputDebugStringW(L"UE4SS module pin failed\\n");
        return;
    }
    UE4SSProgram* program{};
    try
    {
        program = new UE4SSProgram(moduleFilenameBuffer, {}, startup_started);
    }
    catch (...)
    {
        // Construction failed before a usable guard existed. Release only the
        // legacy bootstrap gate; never publish native readiness.
        UE4SSProgram::bootstrap_state.store(Startup::BootstrapState::failed, std::memory_order_release);
        UE4SSProgram::cpp_mods_done_loading.store(true, std::memory_order_release);
        UE4SSProgram::cpp_mods_done_loading.notify_all();
        OutputDebugStringW(L"UE4SS program construction failed\\n");
        return;
    }
    if (program->m_startup_guard->snapshot().native_state == Startup::State::failed)
    {
        return;
    }
    if (HANDLE handle = CreateThread(nullptr, 0, thread_dll_start, program, 0, nullptr); handle)
    {
        CloseHandle(handle);
    }
    else
    {
        program->fail_startup("CreateThread failed for UE4SS initialization");
        return;
    }

    if (s_wait_for_ue4ss)
    {
        // Predicate polling uses the same deadline as the startup worker.
        // Expiration retains program/callback state and does not unload the DLL.
        try
        {
            while (program->m_startup_guard->bootstrap_result() == Startup::BootstrapState::pending)
            {
                program->m_startup_guard->check_phase("bootstrap release");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        catch (const std::exception&)
        {
            UE4SSProgram::bootstrap_state.store(Startup::BootstrapState::failed, std::memory_order_release);
            UE4SSProgram::cpp_mods_done_loading.store(true, std::memory_order_release);
            UE4SSProgram::cpp_mods_done_loading.notify_all();
            OutputDebugStringW(L"UE4SS bootstrap deadline failed\\n");
        }
    }""")
    return text


def initializer_header(text):
    text = replace(text, "#include <atomic>", "#include <atomic>\n#include <memory>\n#include <Unreal/StartupGuard.hpp>")
    return replace(text, "        int64_t SecondsToScanBeforeGivingUp{120};", "        int64_t SecondsToScanBeforeGivingUp{120};\n        std::shared_ptr<Startup::Guard> StartupGuard{};")


def initializer_source(text):
    text = replace(text, "#include <stdexcept>", "#include <stdexcept>\n#include <algorithm>")
    text = replace(text, "    auto IsInitialized() -> std::atomic_bool&", """    static auto CheckStartup(std::string_view phase) -> void
    {
        if (!StaticStorage::GlobalConfig.StartupGuard)
        {
            throw std::runtime_error("Startup guard was not configured");
        }
        StaticStorage::GlobalConfig.StartupGuard->check_phase(phase);
    }

    auto IsInitialized() -> std::atomic_bool&""")
    text = replace(text, "        auto start = std::chrono::steady_clock::now();\n        while (true)", "        while (true)")
    text = replace(text, "            if (ps_scan(ctx, results))", "            CheckStartup(\"PatternSleuth scan\");\n            const bool scan_succeeded = ps_scan(ctx, results);\n            CheckStartup(\"PatternSleuth scan completion\");\n            if (scan_succeeded)")
    text = replace(text, """            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() > UnrealConfig.SecondsToScanBeforeGivingUp)
            {
                throw std::runtime_error{"PS scan timed out"};
            }
""", "")
    text = replace(text, """            auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < UnrealConfig.SecondsToScanBeforeGivingUp)""", "            while (true)")
    text = replace(text, "                ScanResult = ScannerFunction(UnrealConfig);", "                CheckStartup(\"Lua override scan\");\n                ScanResult = ScannerFunction(UnrealConfig);\n                CheckStartup(\"Lua override scan completion\");")
    text = replace(text, "        StaticStorage::bScanFullyCompleted = true;", "        CheckStartup(\"scan publication\");\n        StaticStorage::bScanFullyCompleted = true;")
    text = replace(text, "    auto HookedEngineTick(Hook::TCallbackIterationData<void>&, UEngine*, float, bool) -> void\n    {", """    auto HookedEngineTick(Hook::TCallbackIterationData<void>&, UEngine*, float, bool) -> void
    {
        // Only suppress this loader callback. The original engine tick continues.
        try
        {
            const auto guard = StaticStorage::GlobalConfig.StartupGuard;
            if (!guard) { return; }
            const auto status = guard->snapshot();
            if (status.native_state == Startup::State::failed ||
                (status.native_state == Startup::State::pending && Startup::Clock::now() >= status.deadline)) { return; }
        }
        catch (...) { return; }""")
    text = replace(text, "FNameConstructedHookId = Hook::RegisterFNameConstructorPostCallback([](const auto&, const CharType* String, EFindName) {", """CheckStartup("FName verification hook registration");
        FNameConstructedHookId = Hook::RegisterFNameConstructorPostCallback([guard = StaticStorage::GlobalConfig.StartupGuard](const auto&, const CharType* String, EFindName) {
            // No exception may leave a native hook; late observations grant no readiness.
            try
            {
                const auto status = guard->snapshot();
                if (status.native_state == Startup::State::failed ||
                    (status.native_state == Startup::State::pending && Startup::Clock::now() >= status.deadline)) { return; }
            }
            catch (...) { return; }""")
    text = replace(text, "        StaticStorage::FNameVerificationStatus.wait(false, std::memory_order_acquire);", """        while (!StaticStorage::FNameVerificationStatus.load(std::memory_order_acquire))
        {
            CheckStartup("FName constructor verification");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CheckStartup("FName constructor verification completion");""")
    text = replace(text, "        StaticStorage::GlobalConfig = UnrealConfig;", """        StaticStorage::GlobalConfig = UnrealConfig;
        if (!StaticStorage::GlobalConfig.StartupGuard)
        {
            StaticStorage::GlobalConfig.StartupGuard = std::make_shared<Startup::Guard>(UnrealConfig.SecondsToScanBeforeGivingUp);
        }
        CheckStartup("Unreal pre-initialization");""")
    text = replace(text, "        StaticStorage::GlobalConfig.bIsForcedPreScan = false;", "        CheckStartup(\"Unreal initialization entry\");\n        StaticStorage::GlobalConfig.bIsForcedPreScan = false;")
    text = replace(text, "        for (int32_t Attempt = 0; UObjectArray::GetNumElements() < 10000; ++Attempt)\n        {", """        for (int32_t Attempt = 0; ; ++Attempt)
        {
            CheckStartup("object construction");
            const auto constructed_count = UObjectArray::GetNumElements();
            CheckStartup("object construction observation");
            if (constructed_count >= 10000) { break; }""")
    text = replace(text, "STR(\"Still waiting for object construction, {} objects constructed so far\\n\"), UObjectArray::GetNumElements()", "STR(\"Still waiting for object construction, {} objects constructed so far\\n\"), constructed_count")
    text = replace(text, "        // We will lock here forever if that's not the case.\n        // Consider adding a limit to how long we can wait.", "        // Missing prerequisites fail within the cooperative startup deadline.")
    for phase, statement, predicate in [
        ("KismetStringLibrary", 'KismetStringLibrary = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalNoToStringFromStrings({STR("/Script/Engine"), STR("KismetStringLibrary")}));', "KismetStringLibrary"),
        ("Conv_NameToString", 'FName::Conv_NameToStringInternal = KismetStringLibrary->GetFunctionByName(FName(STR("Conv_NameToString"), FNAME_Find));', "FName::Conv_NameToStringInternal"),
        ("KismetStringLibrary CDO", "FName::KismetStringLibraryCDO = KismetStringLibrary->GetClassDefaultObject();", "FName::KismetStringLibraryCDO"),
    ]:
        text = replace(text, "            " + statement, f'            CheckStartup("{phase} lookup");\n            ' + statement)
        text = replace(text, f"            if ({predicate}) {{ break; }}", f'            CheckStartup("{phase} lookup completion");\n            if ({predicate}) {{ break; }}')
    text = replace(text, "            if (!FName::Conv_NameToStringInternal)\n            {", "            if (!FName::Conv_NameToStringInternal)\n            {\n                CheckStartup(\"Conv_NameToString fallback lookup\");")
    start = text.index("        // Objects that are required to exist before we can continue")
    end = text.index("        auto GetInstanceFromClass =", start)
    required = text[start:end]
    required = required.replace("Hook::AddRequiredObject(", "AddRequiredObject(")
    required = required.replace("Hook::AllRequiredObjectsConstructed()", "AllRequiredObjectsConstructed()")
    intro = """        // Preserve diagnostic names as copied strings before any timeout, so
        // reporting failure never invokes FName/native operations.
        std::vector<StringType> required_labels(Hook::StaticStorage::RequiredObjectsForInit.size(), STR("<previous prerequisite>"));
        auto AddRequiredObject = [&](const std::vector<StringViewType>& names) {
            CheckStartup("required object registration");
            const auto before = Hook::StaticStorage::RequiredObjectsForInit.size();
            Hook::AddRequiredObject(names);
            if (Hook::StaticStorage::RequiredObjectsForInit.size() > before)
            {
                StringType label;
                for (const auto name : names) { if (!label.empty()) { label += STR(":"); } label += name; }
                required_labels.push_back(std::move(label));
            }
            CheckStartup("required object registration completion");
        };
        auto AllRequiredObjectsConstructed = [&] {
            const auto& objects = Hook::StaticStorage::RequiredObjectsForInit;
            const bool complete = std::all_of(objects.begin(), objects.end(), [](const auto& object) { return object.ObjectConstructed; });
            Hook::StaticStorage::bAllRequiredObjectsConstructed = complete;
            return complete;
        };
        auto ReportMissingRequiredObjects = [&] {
            try
            {
            for (size_t index = 0; index < Hook::StaticStorage::RequiredObjectsForInit.size(); ++index)
            {
                if (!Hook::StaticStorage::RequiredObjectsForInit[index].ObjectConstructed)
                {
                    Output::send<LogLevel::Error>(STR("Missing required object: {}\\n"), required_labels.at(index));
                }
            }
            }
            catch (...) { /* Diagnostic failure cannot replace the startup cause. */ }
        };
        try
        {
"""
    required = replace(required, "            {\n                // The control variable", '            {\n                CheckStartup("required object construction");\n                // The control variable')
    required = replace(required, "                    UObject* required_object_ptr =", '                    CheckStartup("required object lookup");\n                    UObject* required_object_ptr =')
    required = replace(required, "                    if (required_object_ptr)", '                    CheckStartup("required object lookup completion");\n                    if (required_object_ptr)')
    required = replace(required, "Hook::StaticStorage::NumRequiredObjectsConstructed >= Hook::StaticStorage::RequiredObjectsForInit.size()", "AllRequiredObjectsConstructed()")
    warn_start = required.index("            if (!Hook::StaticStorage::bAllRequiredObjectsConstructed)\n            {")
    required = required[:warn_start] + """        }
        CheckStartup("required object completion");
        if (!AllRequiredObjectsConstructed())
        {
            throw std::runtime_error("Required engine objects were not constructed");
        }
        }
        catch (...)
        {
            ReportMissingRequiredObjects();
            throw;
        }

"""
    text = text[:start] + intro + required + text[end:]
    final_start = text.index('        Output::send(STR("Constructed {} of {} objects\\n")')
    final_end = text.index('        Output::send(STR("Initializing type system\\n"));', final_start)
    text = text[:final_start] + '        CheckStartup("type system initialization");\n\n' + text[final_end:]
    text = replace(text, "        PostInitialize(UnrealConfig);", '        CheckStartup("Unreal post-initialization");\n        PostInitialize(UnrealConfig);\n        CheckStartup("Unreal post-initialization completion");')
    text = replace(text, "        IsInitialized() = true;", '        CheckStartup("Unreal readiness publication");\n        IsInitialized() = true;')
    return text


def build_metadata(text):
    # Source copies without .git otherwise inherit this workspace's unrelated SHA.
    return replace(text, 'message("UE4SS Version: ${UE4SS_LIB_VERSION_MAJOR}',
                   'set(UE4SS_LIB_BUILD_GITSHA "e3ba101-startup-01")\n\nmessage("UE4SS Version: ${UE4SS_LIB_VERSION_MAJOR}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parents[3]
    source = args.source.resolve(strict=True)
    expected = workspace / "out/dependencies/ue4ss-upstream-e3ba101-bounded-source"
    if source != expected.resolve(strict=True):
        raise ValueError("Only the isolated bounded-source path is allowed")
    patches = [program_header, program_source, entry_source, initializer_header, initializer_source, build_metadata]
    outputs = {}
    records = []
    for (relative, expected_hash), patch in zip(PINNED.items(), patches):
        path = source / relative
        raw = path.read_bytes()
        if digest(raw) != expected_hash:
            raise ValueError(f"Pinned input mismatch: {relative}")
        newline = "\r\n" if b"\r\n" in raw else "\n"
        modified = patch(raw.decode("utf-8").replace("\r\n", "\n")).replace("\n", newline).encode("utf-8")
        outputs[path] = modified
        records.append({"path": relative, "before_sha256": expected_hash, "after_sha256": digest(modified)})
    helper = Path(__file__).with_name("StartupGuard.hpp").read_bytes()
    outputs[source / "deps/first/Unreal/include/Unreal/StartupGuard.hpp"] = helper
    # Validate every patch before writing; a mismatch never leaves a partly patched copy.
    for path, data in outputs.items():
        path.write_bytes(data)
    manifest = {"loader_pin": "e3ba1016562d6c0868c410d0a71e88bfcdbf691b", "unreal_pin": "38e7171e9e8c4a871ff84765848f0290ca34a44e",
                "patches": records, "guard_sha256": digest(helper), "patch_script_sha256": digest(Path(__file__).read_bytes()),
                "runtime_qualified": False, "native_save_tests": 0}
    (source / "startup-patch-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
