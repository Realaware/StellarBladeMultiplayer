# Building the foundation

## Windows

Requirements: Visual Studio 2022 with x64 C++ tools, a Windows SDK, and CMake 3.24 or newer. The Visual Studio bundled CMake is sufficient. The default build fetches no dependencies. The optional `-BuildSaveUi` path downloads and verifies pinned Dear ImGui; see `docs/SAVE_SYSTEM.md`.

```powershell
.\tools\build.ps1 -Configuration Debug -RunHarness
.\tools\build.ps1 -Configuration Release -RunHarness
```

The helper always configures, builds and tests. `-RunHarness` additionally prints the impaired simulation metrics. It returns a failure if any command fails. It does not install anything or change PowerShell execution policy.

Windows backup-worker tests use the installed Windows PowerShell 5.1 to verify disposable archives. Close the game before the fixture backup creation step, as the copy-only helper conservatively rejects a running Stellar Blade process. The runtime re-verifier itself reads only the retained archive and can run without touching live saves.

If CMake/CTest are on PATH, the equivalent manual commands are:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-debug
ctest --preset windows-debug
.\out\build\windows-vs2022\Debug\sbcoop_harness.exe --impaired
```

Use `windows-release` for Release build/test presets. Both configurations share the Visual Studio multi-configuration build directory.

Artifacts are under `out/build/windows-vs2022/<configuration>/`. The harness and tests are executable programs; `sbcoop_core` is a static library. There is no game DLL to install yet.

## Other C++20 toolchains

The generic core can be configured using another suitable generator; the backup worker process bridge and preview are Windows targets:

```text
cmake -S . -B out/build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/native
ctest --test-dir out/build/native --output-on-failure
```

Only the actual recorded toolchains in `docs/TESTING.md` count as verified. Do not infer Linux support for a future game adapter from a portable core.

## Loader integration later

Do not link an arbitrary UE4SS SDK into this build. First identify and pin the exact supported loader/header/compiler/CRT combination. The core's present compiler settings do not attest binary compatibility with the installed loader.

### Upstream UI preparation

The inspected upstream loader uses ImGui 1.92.1. Build the existing shared panel against that pinned version in a separate output directory. With CMake/CTest on PATH, run from the repository:

```powershell
.\tools\fetch-save-ui.ps1 -Version 1.92.1
$uiCandidateSource = (Resolve-Path -LiteralPath '.\out\dependencies\imgui-v1.92.1\imgui-1.92.1').Path
cmake --preset windows-vs2022 -B out/build/windows-vs2022-upstream-ui `
  '-DSBCOOP_BUILD_SAVE_UI=ON' '-DSBCOOP_IMGUI_VERSION=1.92.1' "-DSBCOOP_IMGUI_DIR=$uiCandidateSource"
cmake --build out/build/windows-vs2022-upstream-ui --config Debug
ctest --test-dir out/build/windows-vs2022-upstream-ui -C Debug --output-on-failure
cmake --build out/build/windows-vs2022-upstream-ui --config Release
ctest --test-dir out/build/windows-vs2022-upstream-ui -C Release --output-on-failure
```

Quote the `-D` arguments in PowerShell so dotted version strings and paths remain literal. Both the requested version and source directory must match. This builds the standalone panel/preview only; it does not build, install or qualify UE4SS or an in-game mod. The default build helper continues to select ImGui 1.91.9b explicitly.

### Official loader candidate built locally

The inspected upstream loader `e3ba1016562d6c0868c410d0a71e88bfcdbf691b` and official Unreal archive `38e7171e9e8c4a871ff84765848f0290ca34a44e` now build together in `out/build/ue4ss-upstream-e3ba101`. The source checkout is `out/dependencies/ue4ss-upstream-e3ba101-source`. Exact dependencies, the archive, local compiler/CRT/Rust profile and output hashes are recorded in [the build profile](research/upstream-build-profile.json). This is an unqualified runtime candidate, not a supported game mod or an installation instruction.

The configure used Visual Studio 2022 x64, `-DUE4SS_PROJECTS=UE4SS`, version checks enabled, profiling disabled and `FETCHCONTENT_SOURCE_DIR_ICONFONTCPPHEADERS` pointing to the separately pinned icon-header checkout. The source's Unreal directory contains the verified official archive; its public patternsleuth submodule is checked out at the upstream pin. Rust 1.98.1 is entirely under `out/toolchains/ue4ss-rust`; it is not added to the user's persistent PATH.

To rebuild these existing inspected local sources, run in a fresh PowerShell process with CMake on PATH:

```powershell
$candidateRustRoot = (Resolve-Path -LiteralPath '.\out\toolchains\ue4ss-rust').Path
$env:RUSTUP_HOME = "$candidateRustRoot\rustup"
$env:CARGO_HOME = "$candidateRustRoot\cargo"
$env:PATH = "$candidateRustRoot\rustup\toolchains\1.98.1-x86_64-pc-windows-msvc\bin;$env:PATH"
cmake --build out/build/ue4ss-upstream-e3ba101 --config Game__Shipping__Win64 `
  --target UE4SS proxy --parallel 2 -- /p:CL_MPCount=2
```

These environment changes are confined to that PowerShell process. Do not substitute upstream headers into the installed fork. The candidate configure registers zero CTest tests; the existing project's eight standalone tests and successful loader compilation do not establish game startup, UI interaction, game-thread safety or native save creation. See [qualification evidence and remaining gates](research/upstream-loader-qualification.md).

## Sandbox note

MSBuild may need access to installed SDK metadata outside the project directory. If a sandbox denies that access, use the environment's normal approval mechanism. Do not rewrite SDK locations or weaken project checks to conceal a configuration failure.
