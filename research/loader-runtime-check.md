# Loader re-enable check

Observed September 30, 2026 at approximately 18:23 KST. The user re-enabled the proxy before requesting another check. The assistant did not change loader settings, signatures or installed mods.

- `SB/Binaries/Win64/dwmapi.dll` is present, size 124,928 bytes.
- The installed UE4SS DLL SHA-256 still matches the environment baseline: `F3F0229E8D046824C6D71DB256F1C9BBBDA9652A193643C28FA4E6C329E17670`.
- A fresh launch/log identifies `v3.0.1 Beta #0`, Git SHA `d3d1004`, Game__Shipping__Win64 (MSVC), resolves FText, starts Lua mods/event processing and loads DekCNS.
- A computer-use screenshot showed the main menu and visible game version `1.4.1`. The old 12:04 KST FText crash did not recur. No New Game, Continue or Load action was taken by the assistant.
- The initial launch call did not expose a targetable window before its timeout; subsequent process/window/log checks proved startup success. The game was not blindly relaunched.

This verifies one main-menu startup, not ten-start/exit qualification, teardown, a new C++ mod ABI, game-thread callbacks or native save bindings. The game was left running. Computer use later reported an Escape interruption; a user-authorized resume attempt also reported the stop. No further GUI automation followed that second stop.

Source inspection matched [Chrisr0/RE-UE4SS commit d3d1004](https://github.com/Chrisr0/RE-UE4SS/commit/d3d1004). Source ZIP SHA-256: `2147C3657BC0B11D6554E7CC17379549BED9D4598986B184E5BE819E11165091`. It pins Dear ImGui 1.91.9b and fmt 10.2.1. Unreal submodule commit `02682711c1c58fb167f9f4845119d99ef7575f03` is referenced through Chrisr0/UEPseudo; unauthenticated fetches of the fork and upstream returned 404. Matching header access is unresolved for C++ integration. No guessed replacement declarations were used.

In the inspected source, ExecuteInGameThread dispatches on ProcessEvent and sets a Lua registry promise without independently validating OS game-thread identity in that dispatch function. The documented API is a research lead. A read-only Windows thread-description inventory found render/RHI thread descriptions but no named GameThread; this did not prove a game-thread binding. No object reflection probe was run.

## Header-access follow-up

The user confirmed they do not have access to the matching private UEPseudo repository. The official [UE4SS C++ guide](https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html) lists Epic/GitHub account linking and accepted organization membership as prerequisites. [Epic's access instructions](https://www.unrealengine.com/ue-on-github/) explain that flow. Linking does not itself prove that the old fork/commit remains available; authenticated access must be checked afterward. Account linking/sign-in is for the user to complete.

A read-only MSVC `dumpbin /exports` check of the installed DLL returned zero public `lua_`, `luaL_` or `luaopen_` C API exports (exit 0). C++ Lua-wrapper exports are present. This does not rule out pure Lua mods, but does not establish a compatible public C module boundary for the existing ImGui panel either. No second Lua runtime, guessed C++ declarations or new game-installed probe was introduced.

At this follow-up the game processes were absent and the separate preview process was still running. Process absence alone is not clean-shutdown qualification. No computer-use input was performed in this follow-up.

## Later access and provenance check

The user subsequently linked Epic/GitHub and accepted the invitation. Their authenticated browser could read the private fork and exact pinned tree. Git saved credentials and the connector still failed private access. The user's local branch ZIP is readable but contains `4f723a02...`, rather than the installed source's `02682711...` pin. No mismatched headers were selected. After the user raised concerns about the fork and identified official upstream, the public histories were compared and fork adoption was held for upstream-based qualification. See `loader-source-provenance.md` for revisions, source changes and the fork's limited startup-backup behavior. No loader, mod, save or settings change was made by this inspection.

The later browser download-event method successfully retrieved the official upstream `38e7171...` header archive. A matching upstream `e3ba101...` loader/proxy builds outside the game with recorded input/output fingerprints; see `upstream-loader-qualification.md` and `upstream-build-profile.json`. No runtime experiment was performed with that candidate. The historical installed-fork main-menu observation above must not be counted as an upstream candidate startup or native game-thread/UI qualification.
