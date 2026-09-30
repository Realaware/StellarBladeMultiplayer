# First isolated upstream startup: failed initialization

Startup observed September 30, 2026, Korea Standard Time; post-exit and further source/static checks completed October 1. This experiment advances loader qualification for the multiplayer-save goal; it does not verify native save creation, loaded-save identity, game-thread ownership or the in-game panel.

## Exact inputs and isolation

- Game executable SHA-256: `573AAFF1C9455F85EA6036EF6F6CCB0774DE4164722A296276FBBFC7FA87545C`, 359,186,432 bytes. The previously inventoried Steam build is 24463856.
- Candidate: official loader `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, official Unreal headers `38e7171e9e8c4a871ff84765848f0290ca34a44e`, patternsleuth `1d90b02c7610b595f940af04ce97e0f6feb06923`. See [build profile](upstream-build-profile.json).
- Candidate DLL SHA-256: `EBBF22013EFF1403819E76AFD7042ABB9B76D0C8BBF4D20DB7343297C243F91E`.
- Isolated settings SHA-256: `97044E07879EB8CA2FCBA612394E69076E9BD613720BCC866C874570D12942F5`.
- Retained whole-folder backup was reverified before launch: `C:\Users\snak6\Documents\Stellar Blade Backups\SaveGames_2026-09-30_123508_edb3366c`, archive SHA-256 `1F4C82E58FBEB736E48B3AB963443A66F02E4A83931A2C015FEF9E3523FC855C`. It was neither replaced nor pruned. This does not prove in-game restoration.

The installed legacy proxy was preserved. It contains `override.txt`, but does not contain the newer `--ue4ss-path` option. A byte-identical proxy copy was first tested in an isolated CMake-built host with a harmless marker DLL. Four fresh host processes passed: absent override selects the fixture default; absolute and relative directory overrides select the fixture candidate; an invalid override silently falls back to the fixture default. Each also checked system DWM forwarding. These are routing tests, not game/UE4SS initialization tests. Sources and artifacts are retained under ignored `out/local/loader-route-probe`.

Because a failed route can fall back, the actual loaded DLL path must always be verified. A newly, exclusively created game-directory `override.txt` selected `out/local/upstream-startup-01`, then was removed only after checking its own hash and the loaded module path. Its SHA-256 was `DCA4EC052F644F5119FB9934C1ED89D146940B110A4C2A0A6523E7FA89EBECDF`. Original absence was restored; no installed proxy, loader, settings or mods were replaced. Removing the override does not unload a running candidate.

The staged candidate supplied an explicit isolated settings file, an empty Mods directory and an explicit empty controlling `mods.txt`. Cache, UObject-array listeners/cache, both consoles and crash dumping were disabled. Engine-version overrides remained empty. No layout/vtable overrides or custom signature files were added. User/machine `UE4SS_MODS_PATHS` were absent. The log confirms all root/settings/log/mod paths point to the isolated directory, with no installed mods loaded.

## Startup result

One game process was launched at `21:18:27.705287 +09:00`, PID 37424. The OS module inventory confirmed that this process loaded the isolated candidate DLL. Computer Use's launch returned no targetable game window; the process was not relaunched. Subsequent window-inventory calls reported the user's physical Escape stop, including the user-requested retry. No main-menu screenshot, game input, Continue/New Game/Load action, native probe or clean exit was observed.

The log begins its Phase 2 scan at `21:18:27.7743859` and ends at `21:20:28.8908039` with `Fatal Error: PS scan timed out`, after 251 attempts:

| Result | Confidence and consequence |
| --- | --- |
| EngineVersion 4.26, Shipping, Stats Off | Independent scan output with empty overrides. Recorded evidence, not verification of all native layouts or a supported adapter profile. |
| GUObjectArray: expected at least one value | Required resolver failed. No object-array address was established by this run. |
| ConsoleManagerSingleton: two unique values, `0x14281C1E0` and `0x14381DA90` | Optional resolver ambiguity; neither address was adopted as a native binding. It is not the fatal scan gate. |
| Other required scans found addresses, including GameEngineTick | Research candidates only; no safe callable, hook, game-thread ownership or lifetime was verified. |

The configured scan limit was 30 seconds, but the pinned `UE4SS/src/SettingsManager.cpp:89-96` explicitly changes exactly 30 to 120 as a migration from its old default. `UE4SSProgram.cpp:585` forwards that value; `UnrealInitializer.cpp:494` checks elapsed whole seconds with `>` after each failed scan. The observed approximately 121-second timeout is consistent with that source. The next separate experiment must use a reviewed non-30 limit, such as 20, and record the observed bound; this run did not establish a 30-second bound. The existing experiment settings were left unchanged to preserve evidence.

At `21:30:43 +09:00` the process was still live, and it was again live at the later test/source inspections. At `2026-10-01 01:11:30 +09:00`, PID 37424 was absent and no matching Stellar Blade process existed. No force termination was used. Its exit code, main-menu behavior and native teardown were not observed, so this is process-closure evidence rather than a clean loader-exit qualification.

## Preservation check

The retained baseline files are ignored local evidence, not committed personal saves:

| Local record | SHA-256 |
| --- | --- |
| `out/local/upstream-startup-01/installation-before.json` | `AEBD1F4095C05BED00E71E2FD1D109F561C67DEAD799A5CF073839A9BAD753F7` |
| `out/local/upstream-startup-01/save-hashes-before.json` | `74417FF2CED0B7F92F7F30619CBBE81247E53D1A4236B5A6542ABFA9AE3502E5` |
| `out/local/upstream-startup-01/preservation-live-check.json` | `52E3A227F548CD99C75F440F9B05FB07B2BF9B32194E667B352E1FDE96966E72` |

The current live-process snapshot compared 30 protected installation files and nine SaveGames files by length and SHA-256. All 30 installation files matched. Eight SaveGames files, including every campaign save, matched; no additional SaveGames files were found. `Backup/StellarBladeSetting_Old.sav` changed from `B889609099EF170FA68342629AB8FC3F700E02FA3D3AA426D02985F069FFC145` to `24CE5C3774CF2D268B1F6EAEDF4C57C29390E580FA8E92FD221843323BA73A4C`. Its new content is identical to the existing `StellarBladeSetting.sav`, whose bytes still match the baseline. Both are 162,489 bytes; their observed write times are around 21:21:24. This is consistent with a settings backup refresh; the producing native operation was not traced. Do not claim the entire SaveGames folder was unchanged. No restore/edit was attempted.

The log was 358,132 bytes with equal before/after lengths during a shared read; that observed-byte digest is `7B90FBA6480865E80B512E4C379AC04930D6A1826684672C5A0E4F43B6D351F1`. A normal closed-file read after process exit produced the same digest.

The October 1 post-exit comparison repeated all 30 installation and nine SaveGames checks, with the same result: all installation/campaign files match, only the old settings backup differs, no extra saves exist, and the temporary override is absent. `preservation-after-exit.json` has SHA-256 `68865C0C66E8E6290312A740E3F6B247FB635966E7878C87F659D9B1657D6D56`. The retained whole-folder archive also passed independent re-verification during the diagnostic follow-up. No restore, new backup or save edit was performed.

## Offline scan investigation

A read-only diagnostic inspected the exact on-disk PE and patterns copied verbatim from pinned upstream source. It accesses no live process and makes no native calls. The initial diagnostic assumed a `.pdata` section and failed; reading the PE exception directory instead corrected that assumption. The directory is at RVA `0x15584000`, containing 348,314 function records. Chained x64 unwind records were followed to obtain root functions.

- All six upstream Windows GUObjectArray instruction patterns had zero static matches. This independent check does not implement the resolver's Linux/stat-operand alternatives or prove the complete live resolver behavior.
- `r.DumpingMovie` has one observed LEA-RDX reference at RVA `0x281C315`, in a chained function whose root is RVA `0x281C1E0`.
- `vr.pixeldensity` has one observed LEA-RDX reference at RVA `0x381DB2D`, root RVA `0x381DA90`.
- These reproduce the two distinct addresses in the live ambiguity. Bounded MSVC disassembly shows that the second function reads a global at preferred VA `0x146ED8E98` and, when absent, calls the first function. The first function tests/initializes that same global. HYPOTHESIS: the first is an initializer and the second a console-variable consumer. This is stronger than selecting the first address arbitrarily, but does not verify its calling convention, game-thread use or runtime object layout.
- For comparison only, the previously inspected installed-fork GUObjectArray signature `89 05 ?? ?? ?? ?? 85 DB 7F 36 4C 8D 05 ?? ?? ?? ??` matches once at RVA `0x2B86A83` and resolves its RIP-relative target to RVA `0x6DD7230`. Bounded disassembly confirms a store to that target and nearby allocation-related operations. This is a static candidate, not a verified `FUObjectArray` or approved offset/layout. The fork signature was not installed or executed, and its dependency adoption remains held.

Local diagnostic source/results are `out/local/upstream-startup-01/offline_scan.py` and `offline-scan.json`, SHA-256 `61B3349961A37FFF55FC3C924F7ACC115D69D0919D1EFC6F5EF6681A5A75EC88` and `8560A974133C6085E8FA52694B99FE769495340B994F592E835765D91985A1C4`. `fork-signature-comparison.json` has SHA-256 `28BC5261D3B8DC4F9D44ABC0B40AB2AE6FEC631ABF8334AFC051814F2ED57CF2`. Raw local assembly remains ignored; it is not a generated SDK, memory dump or adapter implementation.

Further source review establishes that the console result is optional and unused in this pin: `deps/first/patternsleuth_bind/src/lib.rs:249-254` passes `true` to optional handling, and only nonoptional errors enter the fatal list at `153-155`. GUObjectArray is nonoptional at `215`. The C++ results field has no consumer, and the Lua override callback only logs it. The console warning needs no repair to resolve this fatal gate. [Array static evidence](upstream-array-static.md) independently cross-checks the array candidate and reveals differing listener positions; [startup bounds](upstream-startup-bounds.md) records later unbounded waits and lifetime limits. Neither finding authorizes a live layout override.

## Validation and next gate

The routing probe's CMake/MSVC Release build and four standalone routing tests passed. In this follow-up, the foundation's existing ImGui 1.92.1 Debug CMake build passed with access to the Windows SDK. CTest initially ran all eight entries: seven passed; `backup_runtime` failed because its fixture-creation helper rejects a running Stellar Blade process. The helper stopped before fixture backup creation. After authoritative process closure, only that entry was rerun with `ctest -C Debug -R '^backup_runtime$' --output-on-failure`, and passed in 3.86 seconds, exit 0. All eight entries have therefore passed across the initial run and this retry; no guard was disabled. Release and native/UI/recovery tests were not rerun.

Next: resolve the exact array/listener layout and startup failure bounds before another runtime experiment; retain the original whole-folder archive. Record actual semantics before adopting a profile. Then qualify ten clean starts/exits, UI context/callback teardown and a verified game-thread entry point. Native inventory, loaded slot, persistent save-instance identity, new-slot creation and mod flags remain pending. A successful source build or unique static pattern cannot substitute for those requirements.
