param([ValidateSet('Configure', 'Build')][string]$Action = 'Configure')
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$sourcePath = Join-Path $workspace 'out\dependencies\ue4ss-upstream-e3ba101-bounded-source'
$buildPath = Join-Path $workspace 'out\build\ue4ss-upstream-e3ba101-bounded'
$baselineBuild = Join-Path $workspace 'out\build\ue4ss-upstream-e3ba101'
$cmakePath = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$rustRoot = Join-Path $workspace 'out\toolchains\ue4ss-rust'
$rustBin = Join-Path $rustRoot 'rustup\toolchains\1.98.1-x86_64-pc-windows-msvc\bin'
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'startup-patch-manifest.json'))) { throw 'Apply the pinned patch first' }
$manifest = Get-Content -LiteralPath (Join-Path $sourcePath 'startup-patch-manifest.json') -Raw | ConvertFrom-Json
foreach ($record in $manifest.patches) {
    if ((Get-FileHash -LiteralPath (Join-Path $sourcePath $record.path)).Hash -ne $record.after_sha256) { throw "Changed patched source: $($record.path)" }
}
if ((Get-FileHash -LiteralPath (Join-Path $sourcePath 'deps\first\Unreal\include\Unreal\StartupGuard.hpp')).Hash -ne $manifest.guard_sha256) { throw 'Changed startup guard' }
$previousPath = $env:PATH
$previousCargo = $env:CARGO_HOME
$previousRustup = $env:RUSTUP_HOME
$previousOffline = $env:CARGO_NET_OFFLINE
try {
    $env:PATH = "$rustBin;$previousPath"
    $env:CARGO_HOME = Join-Path $rustRoot 'cargo'
    $env:RUSTUP_HOME = Join-Path $rustRoot 'rustup'
    $env:CARGO_NET_OFFLINE = 'true'
    if ($Action -eq 'Configure') {
        $arguments = @('-S', $sourcePath, '-B', $buildPath, '-G', 'Visual Studio 17 2022', '-A', 'x64',
            '-DUE4SS_PROJECTS=UE4SS', '-DUE4SS_PROFILERS=OFF', '-DUE4SS_VERSION_CHECK=ON',
            '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', '-DFETCHCONTENT_UPDATES_DISCONNECTED=ON',
            "-DRUSTC_EXECUTABLE=$rustBin\rustc.exe", '-DRust_RESOLVE_RUSTUP_TOOLCHAINS=OFF', '-DRust_RUSTUP_INSTALL_MISSING_TARGET=OFF')
        foreach ($dependency in @('concurrentqueue', 'corrosion', 'fmt', 'glaze', 'glfw', 'imgui', 'imguitextedit', 'polyhook2', 'raw_pdb', 'zydis')) {
            $dependencyPath = Join-Path $baselineBuild "_deps\$dependency-src"
            if (-not (Test-Path -LiteralPath $dependencyPath)) { throw "Missing retained dependency: $dependency" }
            $arguments += "-DFETCHCONTENT_SOURCE_DIR_$($dependency.ToUpperInvariant())=$dependencyPath"
        }
        $arguments += "-DFETCHCONTENT_SOURCE_DIR_ICONFONTCPPHEADERS=$(Join-Path $workspace 'out\dependencies\iconfont-upstream-candidate')"
    } else {
        $arguments = @('--build', $buildPath, '--config', 'Game__Shipping__Win64', '--target', 'UE4SS', 'proxy', '--parallel', '2', '--', '/p:CL_MPCount=2')
    }
    & $cmakePath @arguments
    if ($LASTEXITCODE -ne 0) { throw "CMake $Action failed with exit $LASTEXITCODE" }
} finally {
    $env:PATH = $previousPath
    $env:CARGO_HOME = $previousCargo
    $env:RUSTUP_HOME = $previousRustup
    $env:CARGO_NET_OFFLINE = $previousOffline
}
