# Save management: two slots and one protected main-save backup

## Agreed behavior and current status

The user has checked that Stellar Blade supports multiple saves, with one current saved point per save, and that only the loaded slot is modified. Use this as the project behavior model: **keep the main solo save and a separate co-op save; load the co-op save when playing multiplayer.** Other slots remain untouched.

These observations are user-reported. Implementing the native adapter still requires identifying the real main/co-op slot IDs, their backing files and the actually loaded slot. Do not invent filenames or native functions. There is no separate shared-progression isolation project required by this policy; record any genuinely contradictory implementation evidence if discovered.

The copy-only whole-folder backup helper and independent verifier are implemented. On September 30, 2026, the user-authorized SaveGames backup captured and independently verified nine files without changing the originals. See `research/save-backup-evidence.md`. Native slot management, co-op admission and automatic recovery are not implemented, and no game-load recovery test has been performed.

## Minimum backup requirement

**Before first co-op setup/testing, create at least one verified backup of the entire game save folder, covering users with multiple main saves. Keep that backup permanently protected from automatic overwrite or deletion. If no valid backup exists, block co-op setup/session admission until one is established.** Byte verification is complete for the captured archive; a disposable native restore/load test is still required before claiming tested in-game recovery.

A retained valid backup satisfies subsequent sessions. Do not demand a new copy before every session or after every normal co-op autosave. Check the retained backup's identity, existence and integrity when admitting a session; if it is missing or corrupt, require a valid replacement without destroying any surviving copy. If the main save is also missing, enter explicit recovery rather than creating an empty backup.

Optional later refreshes create new timestamped snapshots while retaining the protected original. An older backup recovers the progress it contains; it does not include solo progress made after that snapshot. Co-op-save backups are optional and not part of the minimum main-save protection requirement.

Normal gameplay never overwrites the main slot or restores an old backup. Any future direct manager edit, import or recovery that would modify existing save data is a separate operation: back up the current affected data first, verify it, and preserve the protected main backup throughout.

## Data to retain

| Item | Purpose |
| --- | --- |
| Main save slot | Normal solo campaign |
| Co-op save slot | Separate native campaign loaded for multiplayer |
| Protected save-folder backup | At least one independent verified archive of all existing saves and folder metadata |
| Local mapping/manifest | Account and slot identity, original relative file path(s), timestamp, sizes, SHA-256 hashes and completed-backup status |
| Optional additional snapshots | Updated solo or co-op recovery points without replacing the protected original |

The user selected a whole-SaveGames-folder archive to cover multiple main saves. Include all files, nested backups and metadata present within that folder; do not expand to unrelated account settings or credentials. Recovery of one slot still requires identifying its needed files; a whole-folder restore can roll back other slots and must be deliberate.

Place the backup outside the active game save location, known cloud-sync roots and this repository. Copy the save; never move it as the backup operation. Do not store backups where a game save operation can overwrite them. Never copy unrelated account credentials or another player's save.

## Initial backup procedure

1. Locate the game's actual SaveGames directory without changing files. Reject a missing or empty source, linked paths or overlapping source/destination. Include every existing slot; selecting a main slot is not required to create the folder archive.
2. With the game closed and relevant writers settled, check space and choose a non-overlapping backup destination. A tool lock alone does not stop native game/cloud writes.
3. Open source files read-only with write/delete sharing denied; if another writer is active, abort. Copy to a uniquely named incomplete ZIP, preserving the directory structure. Flush/close it, reopen and decompress every file for size/SHA-256 comparison. Rehash the held source files and compare the complete before/after inventory; reject a changed snapshot.
4. Persist the completed manifest only after verification. Partial copies, write failures or hash mismatches leave setup blocked and the original untouched.
5. Keep the verified backup without automatic overwrite/pruning. Write `manifest.json`, `VERIFIED.txt` and human-readable `RESTORE.txt`; show its location and creation date. The standalone verifier rejects an `INCOMPLETE.txt` marker, archive hash mismatch, wrong entry set or decompressed file mismatch.

Hash verification proves the copy matches its source. A disposable-save restoration/load test establishes that the chosen file set is recoverable; do not claim that a hash alone proves the game's save format is valid.

## Normal co-op workflow

1. Confirm at least one retained main-save backup remains valid; reuse it rather than copying again.
2. Load the distinct co-op slot through the game's normal menu. Manual selection is sufficient initially.
3. The mod verifies the actually loaded slot before enabling co-op. A menu row number, guessed filename, last-selected setting or unchecked UI checkbox is not a native loaded-slot query.
4. Save/autosave normally into the co-op slot. Recheck the loaded identity across loads/death/reload; if the main or an unknown slot is loaded, disable co-op before further mod gameplay writes.
5. End the session normally. Resume solo later by loading the main slot. No file swap, routine restore, forced post-session backup or cloud-setting change is required.

Designating/creating the co-op slot must not overwrite an occupied main or unrelated slot. Use native creation or an explicitly selected existing separate save. Cloning main progress is optional later work; do not assume a renamed copy automatically creates a valid native slot.

## Missing-save recovery

Recovery is explicit, not a normal session-exit step:

- If the main save disappears or becomes unreadable, close the game and verify the protected backup before restoring.
- Preserve any remaining current files as recovery data before replacing them. Record absent files as absent; a missing source cannot be backed up and must never be substituted with an empty file.
- Resolve active cloud conflicts/writers through normal controls before direct restoration. Do not change cloud settings automatically or guess which conflict copy is correct.
- Restore only the verified main-slot file set from a copy of the backup; never consume the backup or roll back unrelated slots/the entire directory automatically.
- Verify restored bytes and confirm the main save loads. On failure preserve evidence and copies rather than repeatedly overwriting data.

The protected backup remains after successful recovery. Same-drive backup protects against accidental loss/replacement; an independent drive is an optional additional safeguard against drive failure.

## Implementation acceptance

Start with synthetic fixtures, then disposable native saves:

- Missing, incomplete or corrupt backup blocks initial setup/admission; a valid retained backup permits repeated sessions without creating another snapshot.
- Backup uses copy-only operations, stable-source checks and destination hash verification; failures never modify the original.
- The protected original is never automatically overwritten or deleted; optional refreshes create separate snapshots.
- Distinct main/co-op identities are enforced and only the loaded co-op slot permits mod gameplay.
- Normal co-op play preserves the main/other slots and does not trigger swaps, solo restore or mandatory post-session copying.
- Missing-main recovery restores the correct slot and preserves other-slot progress and the backup itself.
- Restore tests load successfully using the minimal documented main-save file set.

Both participants apply this policy locally. Network messages must never choose save paths or replace another player's main campaign. Both-player co-op progression remains a separate later milestone.
