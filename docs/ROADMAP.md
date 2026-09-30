# Roadmap and current status

## Foundation delivered

The independent C++ core, codec, registry, interpolation, bounded queue, fake adapter and in-process regression harness are implemented. A copy-only save-folder backup tool and verifier, generic save creation/admission coordinator, durable flags and a separate Dear ImGui preview are also implemented; a real backup archive has been created. These are reusable groundwork, **not completion of the game or network milestones** below. No native binding, socket or live-save modification exists yet.

One local development PC is available. Real integration testing will later involve friends over an authenticated private VPN. Two game instances on one PC are deferred, not required for progress. A friend test still requires matching verified game/adapter profiles and independent save isolation.

## Gated roadmap

| Milestone | Deliverable / acceptance gate | Current status |
| --- | --- | --- |
| M0 Environment | Reproducible game/loader fingerprints and explicit unknowns | Inventory and upstream/fork relationship recorded; official pinned headers retrieved/inspected, isolated loader/proxy build and fingerprints recorded; runtime profile qualification pending |
| M1 Save safety | Mod-created flagged co-op saves; retained whole-folder backup, loaded-instance guard and recovery | Backup, C++ verifier bridge, coordinator, metadata, generic worker/queues and fixture tests implemented; native creation/query, host integration and game-load recovery pending |
| M2 Loader | Pinned ABI and ten clean starts/exits | One installed-fork startup observed; official upstream candidate builds and loaded in one isolated run, but required scans failed; zero clean candidate starts/exits verified; see `../research/upstream-startup-01.md` |
| M3 Lifecycle | Logging and verified game-thread/teardown callbacks | Pending |
| M4 Player discovery | Correct player/world and lifetime across restarts | Pending |
| M5 Coordinates | Verified native/project transform conversion | Pending |
| M6 Visible proxy | Twenty independent spawn/move/destroy cycles without ownership theft | Pending |
| M7 Real transport | GNS direct-IP harness, admission, mismatch and timeout tests | Codec and simulated core only; sockets/handshake pending |
| M8 Mutual visibility | Both real peers see the other, ten clean reconnects | Pending |
| M9 Smooth motion | Thirty-minute session, twenty reconnects, impaired delivery | Math and simulated traces tested; game/timing integration pending |
| M10 Locomotion | Idle/walk/run/sprint without notify or displacement conflicts | Pending |
| M11 Attack visuals | Unique action instances, no proxy gameplay/camera side effects | Pending |
| M12 Combat feasibility | Native guest attack/damage/targeting plus pre-resolution client suppression | Pending; blocking gameplay gate |
| M13 One enemy | One canonical representation, host AI, matching presentation/health/death | Pending |
| M14 Guest attacks | Host-resolved hits and duplicate-safe results | Pending |
| M15 Player health | Supported nonlethal damage/healing/reactions with host revisions | Pending |
| M16 Death/restart | Coordinated party failure and fresh epoch | Pending |
| M17 World/areas | One interaction and transition pair; ten round trips and interrupted load | Pending |
| M18 Persistence | Both co-op saves retain supported progress; interrupted commits recover | Pending |
| M19 Boss profile | Every supported phase, failure and disconnect validated | Optional after normal enemies |
| M20 UX/package | Host/join/status and reversible installation | Separate save panel/preview builds checked with ImGui 1.91.9b and upstream's 1.92.1; in-game menu host and planned F8 toggle pending |
| M21 Late join | Staged baselines and bounded revision catch-up at safe checkpoints | Deferred |
| M22 Qualification | Two-hour sessions, coexistence, failure/recovery and performance budgets | Pending |
| M23 Test release | Reproducible package matching the tested support matrix | Pending |

## Next order

1. Refresh M0 evidence with the read-only inventory and establish source/ABI provenance.
2. Reuse the protected whole-folder archive, verifier bridge, coordinator and generic runtime queues. Establish verified native creation, loaded-save and persistent-instance bindings, then connect the in-game host/panel and real verifier. Validate creation isolation and recovery with disposable saves. No mandatory per-session copies or routine swaps/restores.
3. Qualify an upstream-based loader/header profile, review necessary Stellar Blade changes and verify the game-thread lifecycle before any native probe or save operation. One successful main-menu startup does not meet M2/M3 acceptance. See `../research/loader-source-provenance.md`.
4. Discover player transform, then test a local visual proxy.
5. Add pinned GNS and a standalone real transport/session harness independently of native hooks.
6. Integrate mutually visible proxies once both branches' prerequisites pass.

Do not expand scope to combat, enemies or progression to compensate for a failed earlier gate. If a capability cannot be verified, keep it disabled and record the failed experiment.
