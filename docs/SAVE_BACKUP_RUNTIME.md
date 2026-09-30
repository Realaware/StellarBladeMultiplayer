# Retained-backup runtime verification

## Implemented boundary

`save::BackupVerifier` is a Windows filesystem-worker component. It launches the project's `tools/verify-backup-worker.ps1` with Windows PowerShell 5.1 using `CreateProcessW` and `-File`. It never calls a game function, reads game memory, writes a save, creates a replacement backup or restores data.

Configuration contains the absolute local worker-script path, selected retained backup directory, expected archive SHA-256 and a bounded deadline (1 ms to 60 seconds; default 30 seconds). These are trusted local configuration, not network-selected paths or scripts. The expected hash must come from the retained backup selected during setup, rather than being reselected from whatever manifest exists at admission.

The worker invokes the existing `Test-SaveBackup` routine: archive SHA-256, exact ZIP entry set, decompressed sizes and per-file hashes. It additionally requires the archive's hash to match the selected identity. It imports its own host's built-in utility module explicitly; inherited PowerShell 7 module search paths otherwise made `Get-FileHash` unavailable in a Windows PowerShell 5.1 child during testing.

## Process and result rules

- The child has no window, profile, interactive input or shell command expression. Windows argument quoting passes spaces, apostrophes, Unicode and `$()` as literal path data.
- Only the output pipe and null input handle are inherited. A job object bounds the verifier's process lifetime and terminates its children on timeout/cleanup.
- Output is capped at 128 bytes. Wrong versions, extra output, truncation, unexpected exits and mismatched identities return an unverified result.
- Verification calls are serialized and produce increasing nonzero check IDs, including failure results. An earlier successful result is never reused by `verify()`.
- The coordinator receives only `BackupCheck` values: state, expected archive hash on success and check ID. Failures contain no personal paths or exception output.

Worker protocol version 1 emits exactly one line:

```text
SBCOOP_BACKUP/1 VERIFIED <64 uppercase hexadecimal SHA-256>  (exit 0)
SBCOOP_BACKUP/1 MISSING                                    (exit 3)
SBCOOP_BACKUP/1 CORRUPT                                    (exit 4)
```

`decode_backup_result` also validates matching exit code and archive identity. The framing is a local worker protocol, not the multiplayer wire protocol.

## Probe and tests

```powershell
.\out\build\windows-vs2022\Debug\sbcoop_backup_probe.exe `
  "$PWD\tools\verify-backup-worker.ps1" `
  'C:\path\to\retained-backup' `
  '<the retained archive SHA-256>'
```

`backup_result` tests framing, archive identity and failure parsing. Windows `backup_runtime` creates disposable archives and exercises the actual subprocess: repeated verification with fresh IDs, missing archives, wrong selected identity, timeout, oversized output, Unicode/metacharacter paths and corruption after an earlier success. Fixtures remain under ignored build output. No personal save is a test fixture.

The retained real nine-file archive passed this C++ bridge on September 30, 2026 with hash `1F4C82E58FBEB736E48B3AB963443A66F02E4A83931A2C015FEF9E3523FC855C`. No live files or retained backup files were changed by that probe. This is archive verification, not an in-game restore/load test.

## Remaining integration

The generic `RuntimeController` now owns a filesystem worker, accepts a verification callback and uses bounded value queues around `SaveSystem`; see `SAVE_SYSTEM.md`. Its regression tests use synthetic backup results. An in-game host must configure the real verifier callback and verified native adapter and request fresh verification at admission. The UI consumes copied status without blocking on archive I/O. That host and the native game adapter are not implemented. A compiled verifier/controller does not enable multiplayer or native creation.
