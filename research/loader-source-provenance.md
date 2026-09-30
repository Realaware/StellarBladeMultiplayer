# UE4SS source provenance and downloaded headers

Observed September 30, 2026. This is source inspection, not binary qualification, a complete security audit or a native binding experiment.

## Upstream and installed fork

The upstream project is [UE4SS-RE/RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS). Its [dependency file](https://github.com/UE4SS-RE/RE-UE4SS/blob/e3ba1016562d6c0868c410d0a71e88bfcdbf691b/.gitmodules) references `Re-UE4SS/UEPseudo`, the Unreal pseudo-code/wrapper submodule. UEPseudo contains headers and supporting C++ implementation; it is not a multiplayer mod or an automatic Stellar Blade SDK. The upstream [C++ guide](https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html) requires initializing the source submodules and obtaining authorized Unreal source access.

Public upstream history was cloned without checkout or recursive submodules into ignored `out/dependencies/ue4ss-upstream-review`. The public Chrisr0 loader fork's history was fetched into that research repository. No downloaded build scripts, binaries or game integration code were run.

| Evidence | Revision |
| --- | --- |
| Upstream main observed by Git | `e3ba1016562d6c0868c410d0a71e88bfcdbf691b` |
| Source revision reported by installed loader log | `d3d10044d12566b869de56164bdaf5dbf36067b8` |
| Shared ancestor of that fork revision and upstream | `8e08d135cba514f53591a5cc3026a5c4c66a0c33` |
| Ancestor's Unreal submodule | `95342f5deecca40308ee4d136932e1a81861d214` |
| Installed fork source's Unreal submodule | `02682711c1c58fb167f9f4845119d99ef7575f03` |
| Observed upstream main's Unreal submodule | `38e7171e9e8c4a871ff84765848f0290ca34a44e` |

Git graph/diff inspection found six fork-side commits since the shared ancestor, including a merge; the aggregate diff changes twelve files, with 887 insertions and 16 deletions. Upstream main has 732 commits beyond the shared ancestor. These counts do not establish binary compatibility. The fork changes the Unreal submodule URL to `Chrisr0/UEPseudo` and its pinned revision, adds Stellar Blade signatures/vtable data and an engine-version override, changes SDK symbol validation and build configuration, and adds startup save backups. Signatures, vtables and the override are source findings, not independently verified game bindings or engine-version evidence.

The [fork source](https://github.com/Chrisr0/RE-UE4SS/commit/d3d10044d12566b869de56164bdaf5dbf36067b8) adds a startup routine that searches account subdirectories for the first `StellarBladeSave00.sav`, copies it into the loader working directory's `SaveBackup` folder with a timestamp and overwrite enabled, and deletes `.sav` copies beyond the three newest in that directory. It does not cover the whole SaveGames folder, independently verify copied bytes or meet this project's retained-backup requirement. Its filename selection does not prove a native slot mapping. Our protected whole-folder archive lives separately; this inspection performed no backup deletion, save edit or loader change.

The log's source identifier and inspected history do not prove that the installed DLL was reproducibly built from that source or contains no other changes. A newer upstream loader/header set must be qualified together before any integration DLL can use it. Do not substitute upstream headers into the installed fork's ABI by assumption.

## Downloaded archive and access

The user downloaded `C:\Users\snak6\Downloads\UEPseudo-sb_fix.zip` (1,869,443 bytes). All 2,219 ZIP entries were opened and decompressed, totaling 8,597,952 bytes. Its README identifies a rehost of the UE4SS generated pseudo-code submodule. No archive code was executed or installed.

- ZIP SHA-256: `DE3F8021F2AE7A99549F181E3301DB060D06FA2DF4AEE1B87D3EB5F9D15A44FC`.
- GitHub ZIP commit comment: `4f723a02ef4890ff0a538b6662f207e4ebfbb18a`.
- This is the `sb_fix` branch archive, **not** the installed loader source's required `02682711...` dependency. It is not selected as a build dependency. Successful decompression and a recorded hash do not authenticate the author or attest safety.

After the user linked Epic/GitHub and accepted the invitation, their authenticated browser could read the private fork and the exact pinned tree. The GitHub connector still returned 404 for that private repository, and Git clone with saved credentials failed authentication. The browser download attempt failed; a subsequent Git device-login attempt was cancelled after the user provided this local archive. Credentials were not read or recorded. This is an access-channel limitation, not evidence that the user still needs to link their accounts.

## Current decision

The user raised concerns about relying on the fork and identified upstream. Use upstream as the source of truth. Adoption of the forked header dependency is on hold while its provenance and necessary compatibility changes are reviewed; no further fork download is required from the user now. Do not infer that a maintainer is malicious or that upstream is automatically game-compatible. The next loader task is to establish an upstream-based, pinned support profile and assess any necessary Stellar Blade changes, including the Unreal submodule diff. Native save creation/query, game-thread entry and in-game UI remain unverified.

The subsequent browser inspection established access to the official pinned UEPseudo tree and read the fork's two-file header diff. Current upstream exposes offset-based accessors and a loader configuration path, yielding a possible route that avoids the old hard-coded demo layout change. Following the initial automated timeout, the browser download-event method retrieved the official `38e7171...` ZIP successfully. Its SHA-256 is `34D2A0A163B33E60687B354897E432143592F57E2937F52387532EFEA37F1E24`; the commit comment matches, and all entries decompressed before bounded/path-checked extraction into ignored output. An isolated matching upstream source configure now succeeds using a workspace-local Rust toolchain. See [Upstream qualification evidence](upstream-loader-qualification.md); source retrieval/build evidence does not qualify a retail layout or ABI.
