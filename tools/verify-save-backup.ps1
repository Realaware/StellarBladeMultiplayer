[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$BackupDirectory)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'SaveBackup.psm1') -Force
Test-SaveBackup -BackupDirectory $BackupDirectory | ConvertTo-Json -Depth 4
