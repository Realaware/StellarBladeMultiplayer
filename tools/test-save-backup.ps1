[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'SaveBackup.psm1') -Force
$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot ('out\save-backup-tests\' + [Guid]::NewGuid().ToString('N'))
$source = Join-Path $fixtureRoot 'source'
$destination = Join-Path $fixtureRoot 'archives'
[IO.Directory]::CreateDirectory((Join-Path $source 'account\Backup')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $source 'empty-directory')) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $source 'account\Slot00.sav'), [byte[]](0..255))
[IO.File]::WriteAllText((Join-Path $source 'account\Backup\Slot00_old.sav'), 'older dummy save')
[IO.File]::WriteAllText((Join-Path $source 'account\설정.sav'), 'dummy settings')
[IO.File]::WriteAllText((Join-Path $source 'steam_autocloud.vdf'), 'dummy metadata')
$hiddenPath = Join-Path $source 'hidden.dat'
[IO.File]::WriteAllBytes($hiddenPath, [byte[]]@())
[IO.File]::SetAttributes($hiddenPath, [IO.FileAttributes]::Hidden)

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "FAIL: $Message" }
}
function Assert-Fails {
    param([scriptblock]$Operation, [string]$ExpectedPattern)
    $failed = $false
    try { & $Operation | Out-Null }
    catch {
        $failed = $true
        if ($_.Exception.Message -notmatch $ExpectedPattern) { throw }
    }
    Assert-True $failed "Expected failure: $ExpectedPattern"
}
function Get-FixtureHashes {
    @(Get-ChildItem -LiteralPath $source -File -Force -Recurse | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{ path = $_.FullName; hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash; modified = $_.LastWriteTimeUtc }
    }) | ConvertTo-Json -Depth 4 -Compress
}

$before = Get-FixtureHashes
$first = New-SaveBackup -SourceFolder $source -BackupRoot $destination
Assert-True ($first.status -eq 'VERIFIED' -and $first.file_count -eq 5) 'Complete backup includes hidden/Unicode/nested files'
Assert-True ($before -ceq (Get-FixtureHashes)) 'Original file contents and timestamps are unchanged'
$manifest = Get-Content -LiteralPath $first.manifest -Raw | ConvertFrom-Json
Assert-True (@($manifest.directories) -contains 'SaveGames/empty-directory/') 'Empty directories are preserved'
Assert-True ((Get-Content -LiteralPath (Join-Path $first.backup_directory 'VERIFIED.txt') -Raw).StartsWith('VERIFIED:')) 'Completion marker is accurate'
$again = Test-SaveBackup -BackupDirectory $first.backup_directory
Assert-True ($again.archive_sha256 -eq $first.archive_sha256) 'Independent archive re-verification passes'
Write-Output 'PASS: full archive, hidden/Unicode/empty entries, source preservation, independent verification'

$second = New-SaveBackup -SourceFolder $source -BackupRoot $destination
Assert-True ($second.backup_directory -ne $first.backup_directory) 'New backup cannot overwrite the first'
Assert-True ((Get-FileHash -LiteralPath $first.archive -Algorithm SHA256).Hash -eq $first.archive_sha256) 'First archive is retained'
Write-Output 'PASS: repeated backup creates a new archive and preserves the original'

Assert-Fails { New-SaveBackup -SourceFolder $source -BackupRoot (Join-Path $source 'nested') } 'must not overlap'
[IO.Directory]::CreateDirectory((Join-Path $fixtureRoot 'empty-source')) | Out-Null
Assert-Fails { New-SaveBackup -SourceFolder (Join-Path $fixtureRoot 'empty-source') -BackupRoot $destination } 'no files'
Assert-Fails { New-SaveBackup -SourceFolder (Join-Path $fixtureRoot 'missing-source') -BackupRoot $destination } 'does not exist|cannot find'
Write-Output 'PASS: overlapping, empty and missing sources are rejected'

$locked = [IO.File]::Open((Join-Path $source 'account\Slot00.sav'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
try { Assert-Fails { New-SaveBackup -SourceFolder $source -BackupRoot $destination } 'being used|sharing|access' }
finally { $locked.Dispose() }
Assert-True ($before -ceq (Get-FixtureHashes)) 'Writer-conflict failure leaves source unchanged'
Write-Output 'PASS: active writer prevents backup without modifying source'

$corrupt = Join-Path $fixtureRoot 'corrupt-copy'
[IO.Directory]::CreateDirectory($corrupt) | Out-Null
[IO.File]::Copy($first.archive, (Join-Path $corrupt 'SaveGames.zip'))
[IO.File]::Copy($first.manifest, (Join-Path $corrupt 'manifest.json'))
$corruptArchive = Join-Path $corrupt 'SaveGames.zip'
$bytes = [IO.File]::ReadAllBytes($corruptArchive)
$bytes[0] = $bytes[0] -bxor 1
[IO.File]::WriteAllBytes($corruptArchive, $bytes)
Assert-Fails { Test-SaveBackup -BackupDirectory $corrupt } 'Archive SHA-256'
Write-Output 'PASS: archive corruption is detected'

$badManifest = Get-Content -LiteralPath (Join-Path $corrupt 'manifest.json') -Raw | ConvertFrom-Json
[IO.File]::Copy($first.archive, $corruptArchive, $true)
$badManifest.files[0].sha256 = ('0' * 64)
[IO.File]::WriteAllText((Join-Path $corrupt 'manifest.json'), ($badManifest | ConvertTo-Json -Depth 8))
Assert-Fails { Test-SaveBackup -BackupDirectory $corrupt } 'Hash mismatch'
Write-Output 'PASS: per-file content mismatch is detected even with a valid archive hash'

[IO.File]::WriteAllText((Join-Path $corrupt 'INCOMPLETE.txt'), 'test interrupted backup')
Assert-Fails { Test-SaveBackup -BackupDirectory $corrupt } 'marked incomplete'
Write-Output 'PASS: incomplete backup is rejected'
Write-Output "All backup fixture tests passed. Fixtures retained under: $fixtureRoot"
