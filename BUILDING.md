# Building the foundation

## Windows

Requirements: Visual Studio 2022 with x64 C++ tools, a Windows SDK, and CMake 3.24 or newer. The Visual Studio bundled CMake is sufficient. There are no fetched dependencies.

```powershell
.\tools\build.ps1 -Configuration Debug -RunHarness
.\tools\build.ps1 -Configuration Release -RunHarness
```

The helper always configures, builds and tests. `-RunHarness` additionally prints the impaired simulation metrics. It returns a failure if any command fails. It does not install anything or change PowerShell execution policy.

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

The core avoids Windows APIs and can be configured using another suitable generator:

```text
cmake -S . -B out/build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/native
ctest --test-dir out/build/native --output-on-failure
```

Only the actual recorded toolchains in `docs/TESTING.md` count as verified. Do not infer Linux support for a future game adapter from a portable core.

## Loader integration later

Do not link an arbitrary UE4SS SDK into this build. First identify and pin the exact supported loader/header/compiler/CRT combination. The core's present compiler settings do not attest binary compatibility with the installed loader.

## Sandbox note

MSBuild may need access to installed SDK metadata outside the project directory. If a sandbox denies that access, use the environment's normal approval mechanism. Do not rewrite SDK locations or weaken project checks to conceal a configuration failure.
