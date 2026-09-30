# Next bounded implementation tasks

Every task must list its objective, prerequisite evidence, allowed files, interfaces, tests, forbidden changes and expected output. Research produces evidence; native implementation consumes VERIFIED findings.

## Research: loader provenance and environment

Refresh the read-only inventory, compare the installed loader with its source/header revision and document ABI requirements. Change only research documents and inventory tooling/tests. Do not launch, inject, replace installed mods, touch saves, or infer engine version from an override. Output an evidence report with unknowns and a proposed exact build profile.

## Research: save boundaries

Read `SAVE_SAFETY.md`. Accept the user's checked behavior: multiple native slots, one saved point per slot, and writes affect only the loaded slot. Identify real main/co-op IDs, backing files, loaded-slot query and required per-slot restore files. Begin read-only and retain the completed verified whole-save-folder archive. Use disposable saves for restore/load experiments. Do not invent filename-to-slot mapping or expand to unrelated account data/per-session backups. Output actual binding evidence and remaining implementation unknowns.

## Save manager: native slot roles after the completed backup helper

The whole-folder ZIP helper, independent verifier and synthetic tests are implemented, and the user-authorized real backup has been verified. Do not recreate this work or replace that archive. Next model distinct account-bound Solo/Co-op identities with a fake loaded-slot provider and reuse the retained backup's verified status. Test wrong/unknown slot rejection and missing-save recovery against fixtures. Allowed changes: dedicated helper/slot-role code, tests and documentation. Do not invent native IDs, require fresh session backups, swap files, restore solo on normal exit, edit formats, import accounts, change cloud settings or prune the protected archive. Native mapping and disposable restore/load validation precede real integration.

## Core: real transport and admission

After choosing a pinned GNS source/dependency revision, implement an isolated `ITransport` wrapper and standalone process harness. Add exact compatibility admission, session IDs, peer ownership, spawn readiness and bounded queues. Update protocol docs and tests. Do not touch game hooks or install anything into the game. Test real loopback sockets and later a friend connection over an authenticated private VPN. Do not describe the current in-memory harness as this deliverable.

## Core: clocks and replication orchestration

Implement four-timestamp clock estimation with uncertainty/slew limits and a reusable replication receiver outside the test harness. Add independently offset/drifting fake clocks, lifecycle ordering, session/ownership checks and epoch-reset tests. Do not change native game code or add combat. Output a tested generic component that can later consume a verified adapter.

## Game integration: read-only adapter

Only after loader ABI, save safety and game-thread lifecycle evidence exists, add a minimal native probe that identifies readiness/world/player and reads transforms. No remote spawning, health writes or networking callbacks may access game objects. Require exact build validation, generation checks and recorded restart/reload experiments.
