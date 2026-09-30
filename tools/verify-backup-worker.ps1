[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BackupDirectory,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9A-F]{64}$')][string]$ExpectedArchiveSha256
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try {
    # Select this host's built-in module explicitly when launched from a parent
    # with a different PowerShell version/module search path.
    Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility') -Force
    if (-not (Test-Path -LiteralPath $BackupDirectory -PathType Container) -or
        -not (Test-Path -LiteralPath (Join-Path $BackupDirectory 'manifest.json') -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $BackupDirectory 'SaveGames.zip') -PathType Leaf)) {
        [Console]::Out.WriteLine('SBCOOP_BACKUP/1 MISSING')
        exit 3
    }
    Import-Module (Join-Path $PSScriptRoot 'SaveBackup.psm1') -Force
    $verifiedBackup = Test-SaveBackup -BackupDirectory $BackupDirectory
    if ($verifiedBackup.archive_sha256 -cne $ExpectedArchiveSha256) { throw 'Retained archive identity mismatch.' }
    [Console]::Out.WriteLine('SBCOOP_BACKUP/1 VERIFIED ' + $verifiedBackup.archive_sha256)
    exit 0
}
catch {
    [Console]::Out.WriteLine('SBCOOP_BACKUP/1 CORRUPT')
    exit 4
}
