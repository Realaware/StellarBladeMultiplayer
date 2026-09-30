[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceFolder,
    [Parameter(Mandatory = $true)][string]$BackupRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'SaveBackup.psm1') -Force
New-SaveBackup -SourceFolder $SourceFolder -BackupRoot $BackupRoot | ConvertTo-Json -Depth 4
