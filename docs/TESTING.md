# Tests and evidence

## Automated tests

`ctest --preset windows-debug` or `windows-release` runs:

1. `core`: eleven test groups covering a fixed wire golden vector, round trips and every truncation, malformed headers/numbers, 10,000 deterministic packet mutations, registry epochs/retired IDs, interpolation/extrapolation, teleport/stale state, quaternion/numeric limits, adapter lifetime/thread ownership, queue closure/capacity and concurrent delivery.
2. `harness_clean`: two fake peers, encoded packets, 20 Hz capture and 100 Hz virtual rendering over ten virtual seconds.
3. `harness_impaired`: the same flow with deterministic delay/jitter, approximately 5% aggregate transform loss, duplication and reordering. Loss is deliberately asymmetric in this schedule.

Both harness cases check bidirectional movement error, bounded interpolation history, despawn cleanup, delayed state after despawn, and old-world rejection. All assertions execute in Release as well as Debug; they do not depend on C `assert`.

The harness is a single process with in-memory delivery and a shared virtual clock. Its nearly zero error on constant-velocity trajectories is a mathematical check, not a game latency or visual-quality benchmark. It does not demonstrate real transport, independently drifting clocks, native animation or game compatibility.

## Recorded local verification

Foundation verification on September 30, 2026 used MSVC 19.44.35228, Visual Studio 2022 x64, CMake 3.31.6-msvc6 and Windows SDK 10.0.26100.0. All three CTest entries passed in both Debug and Release, including all eleven core test groups. The build helper's Release flow and the inventory script against the installed executable/manifest/loader were also exercised successfully. No game was launched or modified. The executable identity is recorded in `research/environment-baseline.md`.

## Later network matrix

Test added RTT 0/30/60/100/150 ms; loss 0/1/3/5%; jitter 0/10/30 ms; duplication/reordering; one-second outages; host/client exit; sleep/resume; queue saturation; and epoch changes during baseline transfer. Full protocol tests can use fake adapters. Actual friends' game sessions remain necessary for integration acceptance.

Before a playable claim, record save hashes, compatibility profiles, area/encounter profiles, camera/input ownership, object lifetime, client suppression, native outcomes, performance and recovery results. Never report a simulated test as a successful in-game experiment.

## Save backup verification and remaining acceptance

Run `.\tools\test-save-backup.ps1` for synthetic tests: hidden/Unicode/empty/nested entries, untouched source contents/timestamps, independent verification, repeat-backup retention, overlapping/empty/missing sources, active-writer rejection, corrupted ZIP, per-file hash mismatch and incomplete marker rejection. All passed on September 30, 2026. A real nine-file archive was created and independently reverified; see `research/save-backup-evidence.md`. Native slot admission and in-game restore tests remain pending.

The matrix is in `SAVE_SAFETY.md`. Retain **at least one verified whole-save-folder backup**, not a fresh copy every session. Future admission must reuse its valid status and enforce native slot-role mapping. Wrong-slot rejection and per-slot missing-save recovery must be tested without changing other slots. Routine co-op exit must not trigger copying, swapping or solo rollback. Optional refreshes preserve the original backup. Successful archive/core tests do not satisfy the native mapping/game-load recovery gate.
