Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression

function Get-StreamHash {
    param([System.IO.Stream]$Stream)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($Stream)).Replace('-', '') }
    finally { $sha.Dispose() }
}

function Assert-GameClosed {
    $running = @(Get-Process -Name 'SB-Win64-Shipping', 'SB', 'StellarBlade' -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        throw 'Stellar Blade is running. Close the game normally before backing up saves.'
    }
}

function Assert-NoLinkAncestors {
    param([string]$Path)
    $candidate = $Path
    while ($candidate) {
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Linked paths are not supported for save backups: $candidate"
            }
        }
        $parent = [IO.Directory]::GetParent($candidate)
        if ($null -eq $parent) { break }
        $candidate = $parent.FullName
    }
}

function Test-PathWithin {
    param([string]$Path, [string]$Root)
    $separator = [IO.Path]::DirectorySeparatorChar
    $prefix = $Root.TrimEnd($separator) + $separator
    return $Path.Equals($Root, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Get-SaveInventory {
    param([string]$Source)
    $prefix = $Source.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $result = @(
        foreach ($item in (Get-ChildItem -LiteralPath $Source -Force -Recurse -ErrorAction Stop | Sort-Object FullName)) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Linked save entry is not supported: $($item.FullName)"
            }
            if (-not $item.FullName.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Save inventory escaped its source directory.'
            }
            $relative = $item.FullName.Substring($prefix.Length).Replace('\', '/')
            [pscustomobject]@{
                path = $relative
                full_path = $item.FullName
                is_directory = [bool]$item.PSIsContainer
                size_bytes = if ($item.PSIsContainer) { [int64]0 } else { [int64]$item.Length }
                last_write_utc = $item.LastWriteTimeUtc.ToString('o')
            }
        }
    )
    return $result
}

function Assert-SafeEntryName {
    param([string]$Name)
    if ([string]::IsNullOrWhiteSpace($Name) -or $Name.StartsWith('/') -or
        $Name.Contains('\') -or $Name.Contains(':') -or
        @($Name.TrimEnd('/').Split('/') | Where-Object { $_ -eq '..' -or $_ -eq '.' -or $_ -eq '' }).Count -gt 0) {
        throw "Unsafe archive entry: $Name"
    }
}

function Assert-ZipContents {
    param([string]$Archive, [object[]]$Files, [string[]]$Directories)
    $expected = @{}
    foreach ($directory in $Directories) {
        Assert-SafeEntryName $directory
        if (-not $directory.EndsWith('/') -or $expected.ContainsKey($directory)) { throw 'Invalid directory manifest.' }
        $expected.Add($directory, $null)
    }
    foreach ($file in $Files) {
        Assert-SafeEntryName $file.archive_path
        if ($file.archive_path.EndsWith('/') -or $expected.ContainsKey($file.archive_path) -or
            $file.sha256 -notmatch '^[0-9A-F]{64}$' -or [int64]$file.size_bytes -lt 0) {
            throw 'Invalid file manifest.'
        }
        $expected.Add($file.archive_path, $file)
    }
    $inputStream = [IO.File]::Open($Archive, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $zip = $null
    try {
        $zip = [IO.Compression.ZipArchive]::new($inputStream, [IO.Compression.ZipArchiveMode]::Read, $true)
        if ($zip.Entries.Count -ne $expected.Count) { throw 'ZIP entry count differs from the manifest.' }
        $seen = @{}
        foreach ($entry in $zip.Entries) {
            Assert-SafeEntryName $entry.FullName
            if (-not $expected.ContainsKey($entry.FullName) -or $seen.ContainsKey($entry.FullName)) {
                throw "Unexpected or duplicate ZIP entry: $($entry.FullName)"
            }
            $seen.Add($entry.FullName, $true)
            $record = $expected[$entry.FullName]
            if ($null -eq $record) {
                if ($entry.Length -ne 0) { throw 'Directory ZIP entry has content.' }
                continue
            }
            if ($entry.Length -ne [int64]$record.size_bytes) { throw "Size mismatch: $($entry.FullName)" }
            $entryStream = $entry.Open()
            try { $actual = Get-StreamHash $entryStream }
            finally { $entryStream.Dispose() }
            if ($actual -cne $record.sha256) { throw "Hash mismatch: $($entry.FullName)" }
        }
    }
    finally {
        if ($null -ne $zip) { $zip.Dispose() }
        $inputStream.Dispose()
    }
}

function Write-NewUtf8File {
    param([string]$Path, [string]$Text)
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
}

function Test-SaveBackup {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$BackupDirectory)
    $root = (Get-Item -LiteralPath $BackupDirectory -Force).FullName
    Assert-NoLinkAncestors $root
    if (Test-Path -LiteralPath (Join-Path $root 'INCOMPLETE.txt')) { throw 'Backup is marked incomplete.' }
    $manifestPath = Join-Path $root 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schema_version -ne 1 -or $manifest.status -ne 'VERIFIED' -or
        $manifest.archive_name -cne 'SaveGames.zip' -or $manifest.archive_sha256 -notmatch '^[0-9A-F]{64}$') {
        throw 'Unsupported or incomplete backup manifest.'
    }
    $archivePath = Join-Path $root $manifest.archive_name
    Assert-NoLinkAncestors $archivePath
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
    if ($archiveHash -cne $manifest.archive_sha256) { throw 'Archive SHA-256 does not match its manifest.' }
    $files = @($manifest.files)
    if ($files.Count -eq 0 -or $files.Count -ne [int]$manifest.file_count) { throw 'Invalid manifest file count.' }
    Assert-ZipContents -Archive $archivePath -Files $files -Directories @($manifest.directories)
    [pscustomobject]@{
        status = 'VERIFIED'
        backup_directory = $root
        archive = $archivePath
        manifest = $manifestPath
        source_directory = $manifest.source_directory
        file_count = $files.Count
        source_bytes = $manifest.source_bytes
        archive_bytes = (Get-Item -LiteralPath $archivePath).Length
        archive_sha256 = $archiveHash
        verification = 'archive SHA-256, complete entry set and decompressed per-file sizes/hashes'
        game_load_recovery_tested = $false
    }
}

function New-SaveBackup {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$SourceFolder,
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )
    Assert-GameClosed
    $sourceItem = Get-Item -LiteralPath $SourceFolder -Force
    if (-not $sourceItem.PSIsContainer -or $sourceItem.PSProvider.Name -ne 'FileSystem') {
        throw 'SourceFolder must be a filesystem directory.'
    }
    $source = $sourceItem.FullName.TrimEnd([IO.Path]::DirectorySeparatorChar)
    $destinationRoot = [IO.Path]::GetFullPath($BackupRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ((Test-PathWithin $destinationRoot $source) -or (Test-PathWithin $source $destinationRoot)) {
        throw 'Save source and backup root must not overlap.'
    }
    Assert-NoLinkAncestors $source
    Assert-NoLinkAncestors $destinationRoot
    foreach ($cloudRoot in @($env:OneDrive, $env:OneDriveConsumer, $env:OneDriveCommercial)) {
        if ($cloudRoot -and (Test-PathWithin $destinationRoot ([IO.Path]::GetFullPath($cloudRoot)))) {
            throw 'Choose a backup root outside the configured OneDrive folder.'
        }
    }
    $inventory = @(Get-SaveInventory $source)
    $sourceFiles = @($inventory | Where-Object { -not $_.is_directory })
    if ($sourceFiles.Count -eq 0) { throw 'Save source has no files; refusing an empty backup.' }
    $streams = [Collections.Generic.List[IDisposable]]::new()
    $records = [Collections.Generic.List[object]]::new()
    $backupDirectory = $null
    try {
        # Hold all source files read-only with write/delete sharing denied. If a
        # cloud client/game already has a writer open, this fails instead of racing it.
        foreach ($file in $sourceFiles) {
            $stream = [IO.File]::Open($file.full_path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
            $streams.Add($stream)
            if ($stream.Length -ne $file.size_bytes) { throw "Source changed: $($file.path)" }
            $hash = Get-StreamHash $stream
            $stream.Position = 0
            $records.Add([pscustomobject]@{
                relative_path = $file.path
                archive_path = 'SaveGames/' + $file.path
                size_bytes = $stream.Length
                sha256 = $hash
                last_write_utc = $file.last_write_utc
            })
        }
        Assert-GameClosed
        $totalBytes = [int64]0
        foreach ($record in $records) { $totalBytes += $record.size_bytes }
        $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($destinationRoot))
        if ($drive.AvailableFreeSpace -lt ($totalBytes * 2 + 16MB)) { throw 'Insufficient free space for a safe backup.' }
        $id = 'SaveGames_' + (Get-Date -Format 'yyyy-MM-dd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        [IO.Directory]::CreateDirectory($destinationRoot) | Out-Null
        $backupDirectory = Join-Path $destinationRoot $id
        if (Test-Path -LiteralPath $backupDirectory) { throw 'Backup destination already exists; refusing overwrite.' }
        [IO.Directory]::CreateDirectory($backupDirectory) | Out-Null
        $incompletePath = Join-Path $backupDirectory 'INCOMPLETE.txt'
        Write-NewUtf8File $incompletePath 'Verification has not completed. Do not restore from this directory.'
        $partialArchive = Join-Path $backupDirectory 'SaveGames.zip.partial'
        $outputStream = [IO.File]::Open($partialArchive, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $zip = $null
        $directories = @('SaveGames/') + @($inventory | Where-Object is_directory | ForEach-Object { 'SaveGames/' + $_.path + '/' })
        try {
            $zip = [IO.Compression.ZipArchive]::new($outputStream, [IO.Compression.ZipArchiveMode]::Create, $true)
            foreach ($directory in $directories) { $null = $zip.CreateEntry($directory) }
            for ($i = 0; $i -lt $records.Count; $i++) {
                $record = $records[$i]
                $entry = $zip.CreateEntry($record.archive_path, [IO.Compression.CompressionLevel]::Optimal)
                $entryStream = $entry.Open()
                try { $streams[$i].CopyTo($entryStream) }
                finally { $entryStream.Dispose() }
            }
            $zip.Dispose()
            $zip = $null
            $outputStream.Flush($true)
        }
        finally {
            if ($null -ne $zip) { $zip.Dispose() }
            $outputStream.Dispose()
        }
        Assert-ZipContents -Archive $partialArchive -Files $records.ToArray() -Directories $directories
        for ($i = 0; $i -lt $records.Count; $i++) {
            $streams[$i].Position = 0
            if ((Get-StreamHash $streams[$i]) -cne $records[$i].sha256) { throw 'Source content changed during backup.' }
        }
        $after = @(Get-SaveInventory $source)
        if (($inventory | ConvertTo-Json -Depth 4 -Compress) -cne ($after | ConvertTo-Json -Depth 4 -Compress)) {
            throw 'Source file/directory inventory changed during backup.'
        }
        Assert-GameClosed
        $archivePath = Join-Path $backupDirectory 'SaveGames.zip'
        [IO.File]::Move($partialArchive, $archivePath)
        $manifest = [ordered]@{
            schema_version = 1
            status = 'VERIFIED'
            created_utc = [DateTime]::UtcNow.ToString('o')
            source_directory = $source
            scope = 'entire SaveGames folder, including nested backup and metadata files'
            archive_name = 'SaveGames.zip'
            archive_sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
            file_count = $records.Count
            source_bytes = $totalBytes
            directories = $directories
            files = $records.ToArray()
        }
        Write-NewUtf8File (Join-Path $backupDirectory 'manifest.json') ($manifest | ConvertTo-Json -Depth 8)
        $restoreNotes = @"
STELLAR BLADE SAVE BACKUP

Original location: $source
Archive: SaveGames.zip
The ZIP contains a top-level SaveGames folder and the complete captured contents.
manifest.json records per-file sizes/hashes and the archive SHA-256.

The original saves were read only. Archive contents were reopened and verified
against the locked source files. This is byte verification, not a game load test.
Keep this folder. The helper never automatically overwrites or deletes backups.

IF RECOVERY IS NEEDED:
1. Close Stellar Blade. Wait for cloud activity to settle; resolve any cloud conflict.
2. Preserve any remaining current saves somewhere separate before replacing files.
3. Extract SaveGames.zip to a NEW temporary folder, not directly over live saves.
4. Locate the needed save and any required metadata. Restoring the entire old folder
   can roll back newer progress in other slots; do that only deliberately.
5. Copy the selected restore set into the original location with the game closed.
   Do not create an extra nested SaveGames\SaveGames directory.
6. Retain this backup and verify the recovered save loads before removing recovery copies.

No automatic restore, cloud-setting changes or save editing was performed.
"@
        Write-NewUtf8File (Join-Path $backupDirectory 'RESTORE.txt') $restoreNotes
        $markerBytes = [Text.Encoding]::UTF8.GetBytes('VERIFIED: archive contents and source snapshot matched. See manifest.json and RESTORE.txt. No game load test was performed.')
        $markerStream = [IO.File]::Open($incompletePath, [IO.FileMode]::Truncate, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $markerStream.Write($markerBytes, 0, $markerBytes.Length); $markerStream.Flush($true) }
        finally { $markerStream.Dispose() }
        # Rename our own completion marker only; no source deletion or cleanup.
        [IO.File]::Move($incompletePath, (Join-Path $backupDirectory 'VERIFIED.txt'))
        return Test-SaveBackup -BackupDirectory $backupDirectory
    }
    catch {
        if ($backupDirectory -and (Test-Path -LiteralPath $backupDirectory)) {
            $failedMarker = Join-Path $backupDirectory 'INCOMPLETE.txt'
            if (-not (Test-Path -LiteralPath $failedMarker)) {
                try { Write-NewUtf8File $failedMarker 'Final verification failed. Do not restore without investigating.' }
                catch { Write-Warning 'Could not persist the incomplete marker; this backup must not be treated as verified.' }
            }
        }
        $suffix = if ($backupDirectory) { " Partial backup retained at: $backupDirectory" } else { '' }
        throw "Backup failed; source files were never modified. $($_.Exception.Message)$suffix"
    }
    finally {
        foreach ($stream in $streams) { $stream.Dispose() }
    }
}

Export-ModuleMember -Function New-SaveBackup, Test-SaveBackup
