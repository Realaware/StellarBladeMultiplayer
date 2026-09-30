# Next bounded implementation tasks

Every task must list its objective, prerequisite evidence, allowed files, interfaces, tests, forbidden changes and expected output. Research produces evidence; native implementation consumes VERIFIED findings.

## Research: loader provenance and environment

The installed loader has one observed fresh main-menu startup, and its matching public source revision was inspected; see `research/loader-runtime-check.md`. Upstream/fork provenance is recorded in `research/loader-source-provenance.md`. The user raised trust concerns, so qualify an upstream-based pinned profile and review necessary Stellar Blade changes before selecting headers/ABI or a runtime. Keep initial evidence collection read-only; do not replace installed mods, touch saves or infer engine version from an override. Runtime experiments require an explicit bounded experiment plan and verified prerequisites.

`research/upstream-loader-qualification.md` records the retrieved official header pin, matching source build and current upstream layout/UI interfaces. The first isolated runtime attempt loaded that candidate, but its required GUObjectArray scan failed; the console warning is optional. Post-exit preservation checks are recorded in `research/upstream-startup-01.md`. A separate cooperative-deadline source candidate now builds, and its helper passes synthetic failure/concurrency tests; see `research/upstream-startup-bounds.md` and its separate candidate profile. It has not run in the game. The next bounded task remains exact-build array/listener evidence and qualification of startup/callback lifetimes, using `research/upstream-array-static.md`. Cache-disabled startup still adds a shutdown listener. Keep all measured addresses/layout hypotheses unadopted until their native semantics and ownership are verified; do not apply the fork's demo layout or mix candidate headers with the installed fork's DLL.

## Research: save boundaries

Read `SAVE_SAFETY.md`. Accept the user's checked behavior: multiple native slots, one saved point per slot, and writes affect only the loaded slot. Identify real main/co-op IDs, backing files, loaded-slot query and required per-slot restore files. Begin read-only and retain the completed verified whole-save-folder archive. Use disposable saves for restore/load experiments. Do not invent filename-to-slot mapping or expand to unrelated account data/per-session backups. Output actual binding evidence and remaining implementation unknowns.

## Save manager: connect the value coordinator to verified native bindings

The retained real backup, C++ verifier bridge, generic save coordinator/runtime queues, durable flag store and separate Dear ImGui preview are implemented; do not recreate this work or replace the archive. Read `SAVE_SYSTEM.md` and `SAVE_BACKUP_RUNTIME.md`, then establish native new-save creation, loaded identity and a persistent save-instance identity that survives autosaves but changes on slot replacement. Qualify the upstream-based loader/header profile and prove a game-thread entry before native calls. Configure the runtime with the real verifier and verified native adapter and host the panel in-game, with a planned F8 toggle. Allowed changes: dedicated adapter/bridge/panel host code, focused tests and evidence/docs. Preserve all existing slots and issue flags only after verified persisted mod-requested new-slot creation. Do not invent native IDs, require fresh session backups, swap files, restore solo on normal exit, edit formats, import accounts, change cloud settings or prune the protected archive. Native creation isolation and disposable restore/load validation precede real multiplayer admission.

## Core: real transport and admission

After choosing a pinned GNS source/dependency revision, implement an isolated `ITransport` wrapper and standalone process harness. Add exact compatibility admission, session IDs, peer ownership, spawn readiness and bounded queues. Update protocol docs and tests. Do not touch game hooks or install anything into the game. Test real loopback sockets and later a friend connection over an authenticated private VPN. Do not describe the current in-memory harness as this deliverable.

## Core: clocks and replication orchestration

Implement four-timestamp clock estimation with uncertainty/slew limits and a reusable replication receiver outside the test harness. Add independently offset/drifting fake clocks, lifecycle ordering, session/ownership checks and epoch-reset tests. Do not change native game code or add combat. Output a tested generic component that can later consume a verified adapter.

## Game integration: read-only adapter

Only after loader ABI, save safety and game-thread lifecycle evidence exists, add a minimal native probe that identifies readiness/world/player and reads transforms. No remote spawning, health writes or networking callbacks may access game objects. Require exact build validation, generation checks and recorded restart/reload experiments.
