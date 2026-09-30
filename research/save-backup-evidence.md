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
