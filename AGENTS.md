# Project engineering rules

Read `README.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md` and the document owning the feature before changing code.

- This is currently a standalone C++ foundation, not a working game mod.
- Preserve existing game installations, user mods, settings and saves.
- Do not fabricate offsets, signatures, SDK types, function names or engine versions.
- Game bindings require build-specific VERIFIED findings and recorded experiments.
- Only a game adapter may know native layouts. All native operations must use a verified game-thread entry point.
- Network/UI workers exchange value data; they must never dereference game objects.
- Never serialize pointers, native structs, arbitrary reflection calls, scripts or asset paths.
- Maintain protocol versioning, size bounds, explicit failures, epochs and lifetime generations.
- Do not implement DRM, authentication, entitlement, anti-cheat or online-service bypasses.
- Save model: the user verified that only the loaded native slot is modified. Keep distinct main/solo and co-op slots, identify their real native IDs and verify the loaded co-op slot before mod gameplay. Preserve all other slots; no routine file swapping or post-session solo restoration.
- Minimum save rule: retain at least one verified whole-SaveGames-folder backup covering every existing main save before first co-op setup/testing, without automatic overwrite/deletion. Reuse that valid backup across sessions; fresh per-session or post-session backups are not required. A missing/corrupt backup blocks admission until corrected. Optional refreshes create separate copies. Before direct modification of existing save data, additionally back up the current affected data; recovery of a missing file preserves remaining data and records the absence. Byte-verified archives do not establish in-game restore success. Follow `docs/SAVE_SAFETY.md`.
- Single-PC dual-game launch is deferred. Use the fake adapter locally; friends can later test actual games over an authenticated private VPN.
- Do not claim the simulated in-process link tests sockets, independent machines or real game behavior.
- Work on one bounded capability at a time and update its documentation and tests.
- Run the applicable CMake build and CTest suite. Record tests not run instead of inventing results.
- Do not commit game assets, executables, personal saves, memory dumps, generated bulk SDK dumps or credentials.
- No agent delegation is requested by this file.
