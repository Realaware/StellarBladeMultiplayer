# First whole-save-folder backup

Date: September 30, 2026. The user explicitly requested a backup in an accessible location before further mod work.

- Source: the current user's `%LOCALAPPDATA%\SB\Saved\SaveGames` folder.
- Destination: the user's Documents folder, under `Stellar Blade Backups`.
- Backup directory name: `SaveGames_2026-09-30_123508_edb3366c`.
- Archive: `SaveGames.zip`; supporting files: `manifest.json`, `VERIFIED.txt`, `RESTORE.txt`.
- Entire folder captured: 9 files, 34,623,942 source bytes.
- Archive size: 1,266,436 bytes.
- Archive SHA-256: `1F4C82E58FBEB736E48B3AB963443A66F02E4A83931A2C015FEF9E3523FC855C`.

Stellar Blade was not running. Sources were opened read-only with write/delete sharing denied while copying and verifying. Every ZIP entry was reopened/decompressed, its size and SHA-256 compared, source streams rehashed, and the full before/after file/directory inventory checked. A separate verifier invocation passed afterward. No live save or cloud setting was modified.

Scope includes both discovered save-numbered files, settings, mod save data, metadata and the existing nested backup directory. Filename numbering has not been mapped to native main/co-op slot identity. No game-load recovery test was performed. This is verified byte preservation, not a claim of tested in-game restoration.

Raw saves and their machine-specific manifest remain outside the repository. Preserve the archive; do not replace it as part of routine development.

## Follow-up integrity check

On September 30, 2026, the user requested another check of the backup workflow. The retained archive passed full archive/entry verification again in PowerShell 7.6.5 and Windows PowerShell 5.1. Its archive hash and file count remain as recorded above. A read-only comparison with the live save folder found all nine files still matching the captured hashes, no missing files and nine current files.

Review reproduced a Windows PowerShell 5.1 compatibility bug: default text decoding misread UTF-8 manifest filenames containing Korean characters. The verifier now explicitly decodes UTF-8. Synthetic tests construct their Unicode filename from code points and assert its exact manifest identity, so both shells test the same filename. All backup fixture tests passed in both shells after the fix, including source preservation, separate-copy retention, active-writer rejection, corruption, per-file mismatch and incomplete-backup rejection. The Debug CMake build and all three CTest entries also passed.

This check did not create a replacement real backup, modify live saves, change cloud settings or perform an in-game restore/load test. Native loaded-slot/session admission remains unimplemented.

## C++ worker bridge verification

The retained archive was independently verified again on September 30, 2026 by `sbcoop_backup_probe` (Debug), which launched the fixed local PowerShell verifier through the new bounded Windows process bridge. It returned `check_id=1`, verified state, failure code 0 and the original archive SHA-256 recorded above. The existing whole-folder and per-entry checks ran; no live save or backup file was changed. No new archive or in-game restore test was performed.

The bridge's disposable archive/process tests and all seven current CTest entries passed in Debug and Release. See `docs/SAVE_BACKUP_RUNTIME.md` and `docs/TESTING.md` for scope and the Release output directory used to preserve the running preview. This evidence does not identify native slots or complete in-game admission.
