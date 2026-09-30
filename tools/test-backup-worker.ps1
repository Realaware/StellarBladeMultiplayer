[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$TestExecutable,
    [Parameter(Mandatory = $true)][string]$FixtureRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility') -Force
Import-Module (Join-Path $PSScriptRoot 'SaveBackup.psm1') -Force
$workerFixture = Join-Path $FixtureRoot ([Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($workerFixture) | Out-Null
# Shell metacharacters and Unicode must reach -File as literal path data.
$unicodeSuffix = [string][char]0xD55C + [char]0xAE00
$sourceFixture = Join-Path $workerFixture ('source $(New-Item sentinel) ''literal'' ' + $unicodeSuffix)
[IO.Directory]::CreateDirectory($sourceFixture) | Out-Null
[IO.File]::WriteAllText((Join-Path $sourceFixture 'fixture.sav'), 'Disposable fixture; not game data.')
$backupFixture = New-SaveBackup -SourceFolder $sourceFixture -BackupRoot (Join-Path $workerFixture ('backups $(New-Item sentinel) ''literal'' ' + $unicodeSuffix))
[IO.File]::WriteAllText((Join-Path $workerFixture 'slow.ps1'), "param(`$BackupDirectory, `$ExpectedArchiveSha256)`r`nStart-Sleep -Seconds 20`r`n")
[IO.File]::WriteAllText((Join-Path $workerFixture 'noise.ps1'), "param(`$BackupDirectory, `$ExpectedArchiveSha256)`r`n[Console]::Out.WriteLine(('X' * 129))`r`n")
& $TestExecutable (Join-Path $PSScriptRoot 'verify-backup-worker.ps1') $backupFixture.backup_directory $backupFixture.archive_sha256 $workerFixture
if ($LASTEXITCODE -ne 0) { throw "Backup worker integration tests failed: $LASTEXITCODE" }
