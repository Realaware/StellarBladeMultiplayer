[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameExecutable,
    [string]$SteamManifest,
    [string]$LoaderDirectory
)

# Read-only inventory. Does not launch/attach, change settings, dump memory,
# inspect saves, or promote any native binding to VERIFIED.
$ErrorActionPreference = 'Stop'

function Get-FileEvidence {
    param([string]$Path)
    $item = Get-Item -LiteralPath $Path
    if ($item.PSIsContainer -or $item.PSProvider.Name -ne 'FileSystem') {
        throw "Expected a filesystem file: $Path"
    }
    $lengthBefore = $item.Length
    $writeBefore = $item.LastWriteTimeUtc
    $hash = Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256
    $item.Refresh()
    if ($item.Length -ne $lengthBefore -or $item.LastWriteTimeUtc -ne $writeBefore) {
        throw "File changed while inventorying: $Path. Retry with the game and updater closed."
    }
    [ordered]@{
        path = $item.FullName
        size_bytes = $lengthBefore
        last_write_utc = $writeBefore.ToString('o')
        sha256 = $hash.Hash
    }
}

$exeItem = Get-Item -LiteralPath $GameExecutable
if ($exeItem.PSIsContainer -or $exeItem.PSProvider.Name -ne 'FileSystem') {
    throw 'GameExecutable must name a filesystem file.'
}
$stream = [System.IO.File]::Open($exeItem.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
$reader = [System.IO.BinaryReader]::new($stream)
try {
    if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) { throw 'Not a valid DOS/PE executable header.' }
    $stream.Position = 0x3C
    $peOffset = $reader.ReadUInt32()
    if ([uint64]$peOffset + 24 -gt [uint64]$stream.Length) { throw 'PE header offset is outside the file.' }
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) { throw 'PE signature is missing.' }
    $machine = $reader.ReadUInt16()
    $executable = Get-FileEvidence -Path $exeItem.FullName
}
finally {
    $reader.Dispose()
    $stream.Dispose()
}

$architecture = switch ($machine) {
    0x8664 { 'x64' }
    0x014C { 'x86' }
    0xAA64 { 'arm64' }
    default { 'unknown' }
}
$executable['pe_machine'] = '0x{0:X4}' -f $machine
$executable['architecture'] = $architecture
$executable['file_version'] = $exeItem.VersionInfo.FileVersion

$manifest = $null
if ($SteamManifest) {
    $manifest = Get-FileEvidence -Path $SteamManifest
    $manifestText = Get-Content -LiteralPath $SteamManifest -Raw
    foreach ($key in @('appid', 'buildid', 'installdir')) {
        $match = [regex]::Match($manifestText, '"' + $key + '"\s+"([^"\r\n]*)"')
        $manifest[$key] = if ($match.Success) { $match.Groups[1].Value } else { $null }
    }
    if ((Get-FileHash -LiteralPath $SteamManifest -Algorithm SHA256).Hash -ne $manifest.sha256) {
        throw 'Steam manifest changed during inspection. Retry after the updater exits.'
    }
}

$loaderFiles = @()
$engineOverride = $null
if ($LoaderDirectory) {
    $loaderItem = Get-Item -LiteralPath $LoaderDirectory
    if (-not $loaderItem.PSIsContainer -or $loaderItem.PSProvider.Name -ne 'FileSystem') {
        throw 'LoaderDirectory must name a filesystem directory.'
    }
    foreach ($name in @('UE4SS.dll', 'UE4SS-settings.ini', 'Mods\mods.txt')) {
        $candidate = Join-Path $loaderItem.FullName $name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $loaderFiles += Get-FileEvidence -Path $candidate }
    }
    $settingsPath = Join-Path $loaderItem.FullName 'UE4SS-settings.ini'
    if (Test-Path -LiteralPath $settingsPath -PathType Leaf) {
        $settings = Get-Content -LiteralPath $settingsPath -Raw
        $major = [regex]::Match($settings, '(?m)^\s*MajorVersion\s*=\s*(\d+)\s*$')
        $minor = [regex]::Match($settings, '(?m)^\s*MinorVersion\s*=\s*(\d+)\s*$')
        $engineOverride = [ordered]@{
            configured_major = if ($major.Success) { [int]$major.Groups[1].Value } else { $null }
            configured_minor = if ($minor.Success) { [int]$minor.Groups[1].Value } else { $null }
            status = 'CONFIGURATION_ONLY_NOT_ENGINE_DETECTION'
        }
        $settingsEvidence = $loaderFiles | Where-Object { $_.path -eq $settingsPath } | Select-Object -First 1
        if ((Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash -ne $settingsEvidence.sha256) {
            throw 'Loader settings changed during inspection. Retry after configuration edits finish.'
        }
    }
}

[ordered]@{
    schema_version = 1
    observed_utc = [DateTime]::UtcNow.ToString('o')
    method = 'read_only_filesystem_inventory'
    executable = $executable
    steam_manifest = $manifest
    loader_files = $loaderFiles
    engine_override = $engineOverride
    capabilities = [ordered]@{
        engine_exact_revision = 'UNKNOWN'
        loader_current_runtime_compatibility = 'UNKNOWN'
        safe_game_thread_callback = 'UNKNOWN'
        visual_proxy = 'UNKNOWN'
        combat_actor = 'UNKNOWN'
        save_isolation = 'UNKNOWN'
    }
} | ConvertTo-Json -Depth 8
