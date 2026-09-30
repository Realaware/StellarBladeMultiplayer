# Multiplayer-save system

## Full requested outcome

The goal requires a co-op mod UI using an external library, a button that creates a new multiplayer save, and a function that identifies the save actually loaded in-game and checks its multiplayer flag. Only newly mod-created saves can become multiplayer saves. Completion requires verified real-game behavior for all three requirements.

Implemented: `save::SaveSystem`, durable `FileFlagStore` intents/flags, a bounded `RuntimeController`, a Dear ImGui panel and a Windows/D3D11 preview. Synthetic creation/admission, worker lifecycle and UI draw tests pass. The preview's simulated provider must be explicitly enabled; its Create button writes fixture metadata, not native save files. The preview still uses its original synchronous fixture coordinator, not the new runtime controller. Interactive preview verification was interrupted by an Escape stop; a user-authorized resume attempt still reported the stop. No successful interactive preview test is claimed.

The Windows backup worker now runs the existing real archive verifier and supplies bounded `BackupCheck` values; the retained real archive passed through it. See `SAVE_BACKUP_RUNTIME.md`.

Pending: the in-game UI host, F8 registration, native new-save creation, native loaded-save query, persistent native-instance identity, production host integration and native recovery/load tests. F8 is the planned menu toggle; it is not currently registered. The full goal remains active and incomplete. After the user raised trust concerns, fork adoption was held for upstream-based provenance/compatibility review. The official upstream-pinned headers and matching source build are available. Its first isolated game run failed the required object-array scan; static research has identified unadopted candidates and a listener-layout mismatch needing verification. Startup bounds/lifetimes also remain unqualified. Resolve these prerequisites before native save integration; see `../research/upstream-loader-qualification.md`.

## Creation transaction and flag

The coordinator consumes immutable values from a future verified game-thread adapter. Rendering never accesses objects. Before creation/admission, a worker must freshly verify the retained whole-folder archive and supply a monotonic check ID and archive hash. `save::BackupVerifier` performs this verification through the existing PowerShell helper. The generic runtime accepts a verifier callback, but no in-game host configures it yet.

Creation requires verified read/create bindings, complete native inventory and a supported new-save context with no loaded gameplay save. An exclusive durable intent records a random 128-bit transaction ID and all occupied identities before native creation is issued. The adapter must recheck the context, create into an unused slot through the game's native flow, and confirm persistence and preservation of existing data.

Only a matching receipt can publish the co-op flag. The created identity must match the account/profile, use a fresh instance and unoccupied slot, appear as the actually loaded save in a newer generation, and add exactly one native save while retaining all previous identities. Failure/interruption preserves the intent and leaves the save unflagged. Restart does not complete unfinished intents automatically.

Identity is `{account, native slot, native save instance, supported profile}`. Its instance must persist through autosave/reload and change when a slot is recreated. These semantics are still UNKNOWN in Stellar Blade. Filename, menu row, timestamp, pointer and whole-file content hash cannot substitute for that evidence. Existing solo saves cannot be promoted merely by adding a flag.

## Admission and persistence

`check_loaded` rejects missing/corrupt/unverified backup evidence, unavailable native bindings, unknown/loading/no-save states, invalid observations, unflagged saves, ambiguous records and metadata errors. Slot replacement cannot inherit a flag because the instance identity must match. Decisions include a load generation. `permit_matches` checks only current identity/generation; a production controller must also freshly verify backup/flag state at admission and revoke permits on loads/errors. No real session currently consumes these decisions.

`FileFlagStore` only writes `.intent`, `.flag` and incomplete `.partial` metadata in its dedicated directory. It exposes no `.sav` path/write operation. The record format is versioned and length-prefixed, with bounded fields/counts, strict flag parsing, controlled ID filenames and rejection of linked paths. Writes use exclusive creation, flush and publication without replacing a previous record. No automatic pruning is implemented. Native save format edits remain absent.

## Runtime controller and thread boundary

`RuntimeController` owns a filesystem worker and invokes the flag store, backup callback and transaction-ID callback only on that worker. The supplied store and callback captures must outlive the controller. A host can supply `[&verifier] { return verifier.verify(); }`; callbacks must have bounded execution and the ID callback must produce a fresh cryptographically random ID in production. Tests use synthetic identities, IDs and backup results.

UI commands and adapter observations/receipts enter a bounded value queue (default capacity 16); creation requests leave through a capacity-one queue. Queue acceptance is not operation success: copied snapshots contain the resulting reason. UI commands carry an expected observation generation; all ingress carries a nonzero controller epoch. Construct a new controller for a new epoch. Old epochs/generations are rejected, and changed data cannot reuse a generation. Producers are serialized so the request fence and queue order agree.

Every queued status-changing request immediately revokes cached admission. The worker rechecks backup evidence on observations, refresh, creation and completion and rejects repeated/non-increasing backup-check IDs. Overflow, malformed values, worker exceptions and closure block permission and native dispatch. The panel's `make_save_panel_model` maps copied runtime status and disables actions during verification or faults. There is no periodic backup watcher: a host must request a fresh refresh at each admission, then consume only the resulting current decision.

The future adapter consumes `pop_creation()` only on its verified game thread, immediately re-captures context/inventory, and rechecks generation/account/profile/occupied slots before a native operation. A popped value is not proof of native readiness. Dispatch must precede a successful receipt; undispatched receipts, cancelled/stale requests and corrupt completion backup evidence cannot flag a save. Cancellation drains stale outbound requests and retains durable intents. `close()` stops and joins the worker and revokes permission; in-progress filesystem calls are allowed to finish, while unfinished creation is abandoned without automatic recovery. This component never dereferences a game object or calls native code. No real multiplayer session consumes its permits yet.

## UI library and preview

The shared panel displays backup, loaded-save and flag status and returns Create/Refresh command enums. The generic runtime provides bounded command/result handling; an in-game host must connect the panel and verified native adapter to it.

The default preview pins [Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b), matching the inspected installed-fork source. The shared panel also supports a separately built [v1.92.1](https://github.com/ocornut/imgui/tree/5d4126876bc10396d4c6511853ff10964414c776) profile, matching upstream loader source `e3ba101...`. The helper verifies the selected archive into ignored output and retains the upstream license. CMake accepts only these two versions and rejects a source directory whose header version differs from the requested one.

| ImGui version | Source ZIP SHA-256 | Scope |
| --- | --- | --- |
| `1.91.9b` | `FD37507C8476A6D14CC7C4B352401F31BCBD0F0D995D35390811E968C466F46E` | Default standalone preview |
| `1.92.1` | `D471FA92A74DA5E9A269BE652EABA0FFF26AEE2E4EF9B399AD32A819A96B504B` | Upstream UI preparation, not a qualified game ABI |

```powershell
.\tools\build.ps1 -Configuration Debug -BuildSaveUi
.\tools\build.ps1 -Configuration Release -BuildSaveUi
.\out\build\windows-vs2022\Release\sbcoop_save_preview.exe
```

The preview cannot connect to Stellar Blade. Its synthetic account/profile/slots and metadata under `out/local/save-ui-preview` exist only to review/test the shared panel. Creating fixture flags does not satisfy native acceptance.

`tools/build.ps1 -BuildSaveUi` explicitly selects the default 1.91.9b profile. Use the separate 1.92.1 build commands in `../BUILDING.md` for upstream preparation. Both versions render the same shared panel and pass headless status/draw checks. The 1.92.1 Win32/D3D11 preview also compiles, but was not launched or interactively qualified. This does not establish that our panel can be loaded into either UE4SS DLL: a future host must use the exact qualified loader's ImGui version, configuration, context, allocators and compiler/CRT profile.

## Remaining work

Qualify an upstream-based loader/header profile, build the runtime UI host/F8 toggle, verify its game-thread entry point, discover native creation/loaded-instance methods across restarts and slot recreation, configure the implemented controller with the real backup verifier and native adapter, then create/load a disposable native co-op save and test all rejection/recovery paths. All native methods and layouts require build-specific evidence first.
