# Compatibility

**No Stellar Blade build is currently supported for in-game multiplayer.** The foundation runs outside the game.

Earlier read-only planning inspection found a Steam installation with app ID 3489700, build ID 24463856 and an x64 `SB-Win64-Shipping.exe`. Those observations identify an inspected installation, not runtime adapter support.

An existing UE4SS setup was present. Its settings force major 4/minor 26, which is configuration, not independent engine detection. A historical log indicated previous loader activity; a fresh runtime test is still required. Preserve all existing user mods and configuration.

## Refresh local evidence

Run `tools/inspect-game.ps1` with an executable path and optional Steam manifest/loader directory. It reads file metadata and hashes, validates PE header bounds, and reports runtime capabilities as UNKNOWN. It does not launch or attach to the game or access saves.

Example (substitute your actual paths):

```powershell
.\tools\inspect-game.ps1 -GameExecutable 'E:\SteamLibrary\steamapps\common\StellarBlade\SB\Binaries\Win64\SB-Win64-Shipping.exe' -SteamManifest 'E:\SteamLibrary\steamapps\appmanifest_3489700.acf' -LoaderDirectory 'E:\SteamLibrary\steamapps\common\StellarBlade\SB\Binaries\Win64\ue4ss'
```

Reports contain local paths. Keep raw machine reports under ignored `local/` if saving them; review before sharing. No report automatically enables a feature.

## Future profiles

Pin executable, loader, proxy DLL, settings/signature configuration, SDK/toolchain/CRT, adapter and gameplay-affecting mod versions. Track areas, actor families and encounters separately. Exact matching is the initial policy. Missing/ambiguous critical signatures fail closed rather than falling back to old addresses.
