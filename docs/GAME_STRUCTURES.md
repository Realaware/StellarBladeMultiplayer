# Game binding status

| Capability | Status | Required proof |
| --- | --- | --- |
| Exact engine revision | UNKNOWN | Corroborated installed-build evidence |
| Reliable loader ABI | UNKNOWN | Pinned headers/runtime/compiler and clean lifecycle tests |
| Game-thread callback | UNKNOWN | Thread and teardown experiments |
| World and local player | UNKNOWN | Ownership/lifetime validation across reloads |
| Native coordinate conversion | UNKNOWN | Measured translation/rotation/origin tests |
| Visual character proxy | UNKNOWN | Independent ownership and safe spawn/destruction |
| Animation control | UNKNOWN | Playback and root-motion/notify side-effect isolation |
| Guest combat actor | UNKNOWN | Independent native action and damage ownership |
| Enemy targeting and suppression | UNKNOWN | Both-player targeting and no unauthorized client outcomes |
| Native main/co-op slot bindings | UNKNOWN | User confirms only the loaded slot changes; identify actual stable IDs, backing files and loaded-slot query |
| Main-save backup/recovery | PARTIAL | Entire SaveGames folder archived and independently hash-verified; disposable restore/load test remains pending. See `research/save-backup-evidence.md` |
| Co-op progression | UNKNOWN | Supported semantic progression operations and both-player persistence experiments |

The interfaces in `include/sbcoop/game/` are project contracts. They are not claims about native classes or functions. Add only VERIFIED bindings with links to their finding records.
