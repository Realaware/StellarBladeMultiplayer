# Local environment baseline

Observed September 30, 2026 using `tools/inspect-game.ps1`. Method: read-only filesystem metadata, PE header inspection and SHA-256 hashing. No process attachment, game launch, native function calls, save inspection or settings edits occurred.

| Fact | Observation |
| --- | --- |
| Distribution | Steam manifest app ID 3489700 |
| Installed build ID | 24463856 |
| Executable relative path | `SB/Binaries/Win64/SB-Win64-Shipping.exe` |
| File size | 359186432 bytes |
| PE machine | 0x8664 / x64 |
| File version string | Empty |
| Executable SHA-256 | `573AAFF1C9455F85EA6036EF6F6CCB0774DE4164722A296276FBBFC7FA87545C` |
| Installed UE4SS DLL SHA-256 | `F3F0229E8D046824C6D71DB256F1C9BBBDA9652A193643C28FA4E6C329E17670` |
| Loader settings SHA-256 | `94A2154B3BFF79FBE75B1F19A21F81CCDA5742B1C39F3CDB2558BEF6B1F2806D` |
| Configured engine override | MajorVersion=4, MinorVersion=26 |

The override is not independent engine detection. These file facts do not prove current runtime loader compatibility or support for multiplayer. A complete future compatibility profile still needs proxy/signature/header/toolchain and gameplay-affecting mod provenance.

UNKNOWN: exact engine revision; safe game-thread callback; player/world bindings; visual proxy construction; independent combat actors; client authority suppression; complete save write set and isolation.

Testing arrangement: one local development PC; friends available for later actual game tests over the internet, initially through an authenticated private VPN. Same-PC dual-game launch is deferred. No second machine was inspected.
