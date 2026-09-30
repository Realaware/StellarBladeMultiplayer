# Architecture

## Implemented foundation

`sbcoop_core` contains generic types, a bounded queue, a packet codec, an entity registry and a snapshot buffer. It has no knowledge of Steam, UE4SS, Stellar Blade memory, files, or native functions.

`IGameAdapter` is a project-owned interface with create/apply/destroy/is-alive operations for visual proxies. These names are not reverse-engineered game functions. The only implementation is `testing::FakeGameAdapter`, which stores values and enforces the constructing thread as its owner.

The harness has two simulated peers and a deterministic in-process link. A scripted setup supplies a nonzero session ID, epoch, entity identity and lifecycle messages. This substitutes for admission and reliable lifecycle delivery solely in tests; it is not a production session manager.

## Ownership and lifetimes

- A future host assigns strictly increasing 64-bit entity IDs; 0 and the maximum value are reserved.
- One registry belongs to one session. A new session gets a new registry.
- Lifecycle spawns must be processed in increasing ID order. Rebinding a retired or lower ID is rejected without an unbounded tombstone collection. Future baseline imports must sort spawn records by ID.
- `advance_epoch` clears both registry maps but retains the ID high-water mark. IDs are not reused across area changes.
- Registry bindings do not prove native object validity. An adapter must validate each handle's generation and lifetime.
- An epoch transition invalidates all old handles and clears all interpolation buffers. Registry removal does not itself destroy native objects.
- The registry and snapshot buffers are game-thread owned. The bounded queue is safe for multiple producers/consumers; it rejects overflow and allows draining after close.

## Motion

Project coordinates use metres, right-handed +X forward/+Y left/+Z up, with normalized x/y/z/w Hamilton quaternions representing active column-vector rotations. Native conversion remains unverified.

`SnapshotBuffer` accepts strictly increasing sequence numbers and timestamps. Its default capacity is 32. It linearly interpolates position and uses shortest-path quaternion slerp. Extrapolation estimates linear velocity from the last two accepted samples, keeps the last rotation, and caps prediction at 100 ms. After the cap it holds the projected endpoint rather than snapping back. Invalid projected coordinates fall back to the latest accepted state.

A greater discontinuity counter clears interpolation history; lower counters and duplicate/stale samples are rejected. Clear the buffer explicitly for a new entity or world. All input times must already use one clock domain. The harness uses virtual common time; there is no clock synchronization implementation yet.

## Intended production integration

Use a validated Stellar Blade-specific UE4SS loader profile and a narrow game adapter. Pin the loader ABI before adding a DLL target. Open-source GameNetworkingSockets over direct IP remains the intended transport; it is not yet a dependency or implementation here.

The initial authority model is hybrid: each owner supplies movement, while the host owns session membership, IDs and accepted shared state. Later the host owns AI, damage, health and whitelisted world results. Clients submit semantic intentions. Deterministic lockstep and rollback are not assumed.

Future receive path: transport -> bounded decoder -> authenticated session/ownership/state validation -> game-thread queue -> entity replication -> game adapter. Parsing alone must never grant permission to spawn or modify an entity.

Future transmit path: game-thread capture -> immutable state/events -> bounded network queue -> serializer -> transport. Coalesce obsolete state; fail a session rather than silently dropping important events.

The copy-only PowerShell save helper archives the entire SaveGames folder into a new timestamped directory and verifies each decompressed file against read-locked sources. A standalone verifier checks archive and per-file hashes from the manifest. It does not modify live saves or implement native slot selection/restoration. The user's confirmed behavior is that only the loaded slot changes; a future adapter must identify the actual co-op slot before enabling multiplayer. A valid protected backup is reused across sessions; no routine swaps/restores or per-session copies. Direct save edits/recovery remain separate operations, and network messages never select filesystem paths. See `SAVE_SAFETY.md`.

## Explicitly absent

No sockets, GNS, handshake/authentication, spawn-ack barriers, production replication manager, clock sync, logging backend, UI, game hooks, native adapter, combat, enemy AI, save writes, transitions or progression implementation. These require separate tasks and evidence gates.
