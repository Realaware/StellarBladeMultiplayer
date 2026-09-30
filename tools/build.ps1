[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$RunHarness,
    [switch]$BuildSaveUi
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakePath = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }

if (-not $cmakePath) {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
        throw 'CMake was not found. Install Visual Studio 2022 Desktop development with C++ and CMake tools, or put CMake on PATH.'
    }
    $installPath = & $vswherePath -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installPath) {
        throw 'Visual Studio 2022 with the x64 C++ toolchain was not found.'
    }
    $cmakePath = Join-Path $installPath.Trim() 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}

if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
    throw "CMake was not found at $cmakePath. Add the Visual Studio CMake tools component."
}
$ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctestPath -PathType Leaf)) { throw "CTest was not found at $ctestPath." }

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE."
    }
}

$preset = 'windows-' + $Configuration.ToLowerInvariant()
Push-Location -LiteralPath $repoRoot
try {
    $configureArguments = @('--preset', 'windows-vs2022')
    if ($BuildSaveUi) {
        & (Join-Path $PSScriptRoot 'fetch-save-ui.ps1')
        $uiSourceDirectory = Join-Path $repoRoot 'out\dependencies\imgui-v1.91.9b\imgui-1.91.9b'
        $configureArguments += @('-DSBCOOP_BUILD_SAVE_UI=ON', '-DSBCOOP_IMGUI_VERSION=1.91.9b', "-DSBCOOP_IMGUI_DIR=$uiSourceDirectory")
    }
    Invoke-Checked -Executable $cmakePath -Arguments $configureArguments
    Invoke-Checked -Executable $cmakePath -Arguments @('--build', '--preset', $preset)
    Invoke-Checked -Executable $ctestPath -Arguments @('--preset', $preset)
    if ($RunHarness) {
        $harnessPath = Join-Path $repoRoot "out\build\windows-vs2022\$Configuration\sbcoop_harness.exe"
        Invoke-Checked -Executable $harnessPath -Arguments @('--impaired')
    }
}
finally {
    Pop-Location
}
