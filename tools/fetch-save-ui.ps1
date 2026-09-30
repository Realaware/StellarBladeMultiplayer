[CmdletBinding()]
param(
    [ValidateSet('1.91.9b', '1.92.1')]
    [string]$Version = '1.91.9b'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $repoRoot 'out\dependencies'
[IO.Directory]::CreateDirectory($dependencyRoot) | Out-Null
$archivePath = Join-Path $dependencyRoot "imgui-v$Version.zip"
$archiveHashes = @{
    '1.91.9b' = 'FD37507C8476A6D14CC7C4B352401F31BCBD0F0D995D35390811E968C466F46E'
    '1.92.1' = 'D471FA92A74DA5E9A269BE652EABA0FFF26AEE2E4EF9B399AD32A819A96B504B'
}
$expectedHash = $archiveHashes[$Version]
if (-not (Test-Path -LiteralPath $archivePath)) {
    Invoke-WebRequest -Uri "https://codeload.github.com/ocornut/imgui/zip/refs/tags/v$Version" -OutFile $archivePath
}
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -cne $expectedHash) {
    throw 'Dear ImGui archive hash mismatch. Existing files were retained for inspection.'
}
$extractRoot = Join-Path $dependencyRoot "imgui-v$Version"
if (-not (Test-Path -LiteralPath $extractRoot)) { Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot }
Write-Output "Dear ImGui $Version verified. Source: $extractRoot\imgui-$Version"
