# Upstream startup deadlines and lifetime gates

Source reviewed October 1, 2026, for the multiplayer-save loader prerequisite. Candidate: official loader `e3ba101...` and official Unreal dependency `38e7171...`, as pinned in [the build profile](upstream-build-profile.json). The table describes the pristine source under ignored `out/dependencies/ue4ss-upstream-e3ba101-source`. The initial review made no edits or native observations. A separate patched source candidate has subsequently built; its actual scope and limits are recorded below.

The failed first scan eventually returned, but **the candidate has no demonstrated end-to-end startup or unload bound**. Its scan setting alone cannot qualify a bounded runtime experiment. Changing exactly 30 seconds to 20 avoids the legacy 30-to-120 migration, but cannot bound the later phases.

| Source | Current behavior | Required failure behavior for a future candidate |
| --- | --- | --- |
| `UE4SS/src/main_ue4ss_rewritten.cpp:51-60` | Creates a startup thread using a program pointer, discards its handle, and may wait indefinitely for the bootstrap flag. | Publish a terminal result on success, early error and thread-creation failure. Releasing the main thread must not assert native readiness or permit late initialization. |
| `deps/first/Unreal/src/UnrealInitializer.cpp:482-499` | PS retries check elapsed whole seconds after a failed synchronous scan. A late successful result is accepted. | Validate a bounded positive configuration and check a monotonic deadline before/after attempts. Expiration fails closed. A retry deadline cannot interrupt one blocked synchronous scan. |
| Same file, `547-576` | Lua override scanning has a separate budget and 50 ms waits. | Share the startup deadline and retain fatal checks; do not use overrides to disguise unverified array/layout data. |
| Same file, `736-747` | FName verification waits indefinitely on an atomic flag. | Use a timed predicate; never set verified merely to release the wait. Fail before later FName construction/search. |
| Same file, `797-804` | Waits for at least 10,000 objects without a timeout. | Preserve the threshold and fail on expiration; do not lower it without evidence. |
| Same file, `810-849` | Repeated Kismet object/function/CDO lookups can wait indefinitely. | Preserve identity/readiness prerequisites and fail before dereference or the next stage. |
| Same file, `903-961` | Up to 2,000 iterations sleeping 250 ms; missing objects produce warnings before further work. | Fail with the unresolved prerequisite names before CDO/vtable lookup and hook registration. The current final check at `1180-1188` occurs too late. |
| `UE4SS/include/UE4SSProgram.hpp:28-42` | ImGui context setup spins until a context exists. | Keep Mods empty for current qualification; a future UI host needs bounded readiness and safe publication of context/allocators. |

The bootstrap flag is released in `UE4SSProgram.cpp:862-878` before normal Unreal initialization. It is not a successful native-initialization result. A future host must distinguish bootstrap completion, native readiness and terminal failure.

## Failure and lifetime constraints

FName verification installs a persistent readonly callback, rather than a once-only callback (`Hooks/Hooks.hpp:76-86`). Normal removal occurs in `HookedEngineTick` at `UnrealInitializer.cpp:113-119`; timing out before that hook is registered leaves that cleanup path unqualified. Callback removal can itself wait for active executors (`Hooks/Internal/DetourInstance.hpp:348`, `Metadata.hpp:102-108`). A timeout must not free callback state, force-unload the DLL, or claim safe teardown while a callback/startup thread may still execute.

`UE4SSProgram::init` catches a runtime error and returns; `thread_dll_start` logs it. That does not close the game or delete the shared program. Explicit unload deletes the program through `static_cleanup`, but the startup thread handle has already been discarded. Normal process termination takes a different DllMain path. Process absence after the first failed run is therefore distinct from qualified initialization/teardown.

Object traversal can also block inside one lookup when layouts are wrong: outer chains and native field traversal do not have proven bounds. Timer checks between calls supply cooperative retry bounds, not hard guarantees for arbitrary native traversal. [Exact array/listener layout](upstream-array-static.md) remains a separate prerequisite; cache-disabled startup still writes a shutdown listener.

## Next bounded implementation and verification

Before another native trial, prepare a source-only candidate with one monotonic initialization deadline, explicit terminal bootstrap/native states, and rejection of late readiness. Bound FName/object waits without skipping prerequisites. Fail missing required objects before any dependent CDO/vtable operation. Retain callback/program state after failure until normal process closure unless teardown qualification proves safe release.

Meaningful synthetic cases must cover immediate readiness, never-ready failure, success arriving at/after the deadline, caller failure releasing the bootstrap gate, late completion after failure, and prevention of downstream initialization. Build fingerprints must record every actual patch; prior official-file comparison only describes the original retrieved inputs. The helper tests below cover these state contracts, but not actual Windows thread-creation failure or native hook execution. Existing foundation tests do not prove these native loader behaviors, and no multiplayer save operation may be enabled by a timer or source review.

## Isolated source candidate, October 1

The tracked patch material is in `research/patches/upstream-startup`. `apply_candidate.py` accepts only the separate `out/dependencies/ue4ss-upstream-e3ba101-bounded-source` directory and checks six exact input hashes before writing any patched source. The original source/build are retained. The original DLL/proxy hashes were checked again and still match the original profile. Private source, binaries and build logs remain ignored. [The new profile](upstream-startup-candidate-profile.json) records patched-source, helper, script, log and binary fingerprints separately from the pristine build.

The loader and matching Unreal dependency now share one `Startup::Guard`. Its monotonic start time is captured before program construction; after settings parsing the configured 1–600-second budget uses that same start time. The existing legacy setting migration remains unchanged. This is a cooperative budget: settings parsing, mod callbacks, synchronous scanning and native traversal can still block inside one call.

Bootstrap release leaves native startup pending. Failure is terminal and releases bootstrap waiters; a pending-to-released compare/exchange cannot overwrite failure. Constructor/parser errors, `CreateThread` failure, initialization exceptions and update-thread startup errors publish failure. Readiness is issued after real prerequisites and program-start callbacks finish, before the long-lived event loop. Program-start exceptions previously swallowed by its `TRY` wrapper now propagate. This source readiness is not VERIFIED loader qualification, a verified game-thread entry, or permission to create/admit a save.

PS/Lua scan attempts check before and after synchronous work, so late success is rejected. FName verification polls its atomic predicate with the shared deadline; callbacks retain shared guard ownership and return on failed/expired startup. The 10,000-object threshold and all Kismet identities remain. Required-object waits preserve the prerequisite vector rather than calling the helper that erases entries. Completion derives from every entry's constructed flag, including empty-list success. Copied diagnostic strings remain aligned with the unchanged vector; reporting cannot mask the original failure. Missing prerequisites fail before dependent CDO/vtable operations and hook registration. No retail address, member layout, signature override or native save binding was adopted.

The Windows startup entry uses `DWORD WINAPI(void*)`. Before program construction/hooks, it pins this research DLL with `GetModuleHandleExW`; pin failure prevents initialization. **Explicit unload is unsupported for this candidate.** Failed programs, callbacks and guard state are retained for process closure. No timeout cleanup deletes them or forcibly removes hooks. Late engine-tick callbacks check failed/expired state before loader bookkeeping/cleanup, while the original engine call continues. A callback already past that check can still enter the unbounded executor-drain path: pinning does not prove native teardown or a hard deadline.

`build_candidate.ps1` uses the retained dependency directories, disconnected FetchContent and offline Cargo, with process-local toolchain environment settings. The source-copy version label is explicitly `e3ba101-startup-01`; otherwise upstream's Git lookup picked up this workspace's unrelated parent commit. The additional metadata patch is fingerprinted. Both `UE4SS` and `proxy` built successfully in `Game__Shipping__Win64` with MSVC 19.44.35228, SDK 10.0.26100.0, CMake 3.31.6 and local Rust 1.98.1. Existing upstream warnings remain. The first full build exposed Unreal's `check` macro colliding with the helper method; renaming it to `check_phase` and adding a macro-context compilation regression fixed the successful final build.

The standalone Debug helper build and CTest passed: one entry, ten synthetic cases, 128 concurrent failure/readiness races, 0.04 seconds. Cases include budget/deadline arithmetic, immediate readiness, bootstrap/native separation, never-ready expiry preventing a dependent stage, equality/late readiness rejection, caller failure, late completion, failure after readiness and concurrent terminal failure. The upstream candidate's `ctest -N` registers zero entries. These are source compilation and synthetic state checks only. The unchanged foundation/UI suites and a Release helper build were not rerun for this capability; no in-game startup, UI, native save or recovery test ran.

From the workspace, after preparing a separate pristine source copy without `.git` or Rust `target` directories:

```powershell
python research/patches/upstream-startup/apply_candidate.py --source out/dependencies/ue4ss-upstream-e3ba101-bounded-source
.\research\patches\upstream-startup\build_candidate.ps1 -Action Configure
.\research\patches\upstream-startup\build_candidate.ps1 -Action Build
cmake -S research/patches/upstream-startup -B out/build/upstream-startup-guard-test -G "Visual Studio 17 2022" -A x64
cmake --build out/build/upstream-startup-guard-test --config Debug --parallel 2
ctest --test-dir out/build/upstream-startup-guard-test -C Debug --output-on-failure
```

The actual run used the bundled Python and Visual Studio CMake/CTest absolute paths, as available locally. Before another native trial, exact array/listener layouts, controlled startup behavior, callback/thread ownership and shutdown still require qualification. Computer Use remains stopped after the human Escape interruptions; no game relaunch or workaround occurred in this source-only follow-up. Native save creation, loaded-instance queries and the in-game panel remain pending, and the full save-system goal is incomplete.
