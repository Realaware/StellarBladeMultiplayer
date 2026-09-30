# Stellar Blade Co-op — foundation

This repository is the starting point for a private co-op mod for the PC version of Stellar Blade. **It is not yet a loadable or playable multiplayer mod.**

## What works now

- A dependency-free C++20 core with a Visual Studio 2022 x64 build.
- Versioned binary spawn, despawn and transform messages with bounded validation.
- Host-style monotonic entity IDs and opaque, generation-checked object handles.
- World-epoch invalidation and stale-spawn rejection.
- Position interpolation, shortest-path quaternion interpolation, and capped extrapolation.
- A bounded thread-safe queue with explicit overflow and shutdown behavior.
- A fake game adapter that rejects off-thread access and invalid handles.
- An in-process two-peer simulator with delay, loss, duplication, reordering and cleanup checks.
- Unit/regression tests and a read-only installation inventory script.
- A copy-only whole-save-folder ZIP backup helper, independent archive verifier, and synthetic backup tests. Existing backups are never overwritten or pruned by these tools.
- A save coordinator, durable co-op flag metadata and an optional Dear ImGui save UI preview. The preview uses simulated identities; native save creation and loaded-save queries remain pending. See [Save system](docs/SAVE_SYSTEM.md).
- A Windows C++ worker bridge that re-verifies a selected retained backup using the real archive verifier, with bounded results and process lifetime. See [Backup runtime](docs/SAVE_BACKUP_RUNTIME.md).
- A generic save runtime worker with bounded command/creation queues, generation/epoch fences, cancellation and copied UI status. Tests use synthetic adapter observations; no in-game host configures it yet.
- Two pinned Dear ImGui panel builds: the default 1.91.9b preview and a separate 1.92.1 profile for upstream UE4SS preparation. Neither is an in-game host; see [Building](BUILDING.md).

The simulator exchanges encoded packets in memory. It does **not** open sockets, launch Stellar Blade, access game memory, inject a DLL, modify saves, or test two real game clients.

## Build and run

Install the Visual Studio 2022 **Desktop development with C++** workload, including a Windows SDK and CMake tools. From PowerShell in this repository:

```powershell
.\tools\build.ps1 -Configuration Debug -RunHarness
```

The script discovers Visual Studio's bundled CMake when CMake is not on `PATH`, builds, runs all tests, and optionally runs the impaired-delivery simulation. The default build downloads no third-party packages. `-BuildSaveUi` additionally fetches and verifies pinned Dear ImGui source and builds a separate preview window; see [Save system](docs/SAVE_SYSTEM.md). See [BUILDING.md](BUILDING.md) for manual commands and Release builds.

## Development target

The first in-game target is mutual character visibility with smooth position/rotation replication. Combat is later, gated by verified game behavior.

Current testing arrangement: one local development PC, with friends available for later internet tests through an authenticated private VPN. Running two game instances on one PC is deferred and is not a project blocker. Local simulated peers remain the first test tier.

The eventual first playable release targets one supported area and a small enemy whitelist. Both players should retain supported progress in **separate co-op saves**, with matching supported checkpoints; ordinary campaign saves remain isolated.

Save management uses **separate native solo and co-op slots**; the user confirmed that only the loaded slot is modified. Multiplayer requires a new save created through the mod and flagged only after verified native creation. Load that co-op save for multiplayer and solo for normal play. Keep **at least one verified backup of the entire SaveGames folder** before first co-op setup, covering every existing main save and its metadata, and reuse it across sessions. Routine swaps, solo restores and fresh per-session backups are unnecessary. A real folder backup has been created and independently byte-verified; native creation, loaded-save identity queries and in-game recovery validation remain pending. See [Save safety](docs/SAVE_SAFETY.md).

### Save backup tools

With the game closed, create a new backup in an accessible folder outside the live saves:

```powershell
.\tools\backup-saves.ps1 -SourceFolder "$env:LOCALAPPDATA\SB\Saved\SaveGames" -BackupRoot "$env:USERPROFILE\Documents\Stellar Blade Backups"
.\tools\verify-save-backup.ps1 -BackupDirectory 'C:\path\to\the\created\backup-directory'
.\tools\test-save-backup.ps1
```

The backup directory contains `SaveGames.zip`, `manifest.json`, `VERIFIED.txt` and `RESTORE.txt`. The ZIP includes hidden files, nested existing backups, metadata and empty directories. Source files are held read-only while copying and verifying; a running game, conflicting writer, changed inventory or verification failure aborts. Failed outputs stay marked incomplete. Re-verification checks the archive and its contents without touching live saves. No automatic restore is implemented.

## Read next

- [Architecture](docs/ARCHITECTURE.md) and [wire protocol](docs/NETWORK_PROTOCOL.md)
- [Research rules](docs/REVERSE_ENGINEERING.md) and [game structures](docs/GAME_STRUCTURES.md)
- [Save safety](docs/SAVE_SAFETY.md) and [compatibility](docs/COMPATIBILITY.md)
- [Save coordinator and UI status](docs/SAVE_SYSTEM.md)
- [Roadmap and current gates](docs/ROADMAP.md)
- [Testing](docs/TESTING.md), [debugging](docs/DEBUGGING.md), and [next bounded tasks](docs/TASKS.md)

No game-specific offsets, classes, functions or engine bindings have been implemented. No existing game installation or mod configuration has been changed by this foundation.

A user-requested fresh startup reached the main menu with the re-enabled installed UE4SS loader. This establishes one startup observation, not loader qualification or save integration. See [Runtime check](research/loader-runtime-check.md).

The official upstream UE4SS project and the installed Stellar Blade fork's source relationship have been checked. Forked dependency adoption is held for upstream-based provenance/compatibility review; see [Source provenance](research/loader-source-provenance.md).

A pinned official upstream loader/proxy and matching official Unreal headers now build in ignored project output, with dependency/binary fingerprints recorded. One isolated candidate startup confirmed its DLL loaded, then failed the required object-array scan; an optional console scan also warned. The candidate remains unqualified; see [First upstream startup](research/upstream-startup-01.md) and [Upstream qualification](research/upstream-loader-qualification.md).
