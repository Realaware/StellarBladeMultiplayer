# Retail object-array candidate: static evidence only

Observed October 1, 2026. This is an exact-build offline investigation for the multiplayer-save loader gate. **No native binding or runtime layout is VERIFIED. No address/signature/layout override was installed.** The official candidate and retained backup remain those recorded in [the first startup](upstream-startup-01.md).

Input executable SHA-256 is `573AAFF1C9455F85EA6036EF6F6CCB0774DE4164722A296276FBBFC7FA87545C`; preferred image base is `0x140000000`. Addresses below are RVAs, unless stated otherwise. They were measured from this file, not inferred from a demo or copied into an adapter.

## Independent references to the candidate

The previously inspected fork signature matches once at `0x2B86A83` and resolves its RIP-relative store target to `0x6DD7230`. Current official Windows patterns have zero static matches. A unique store alone does not identify an object array.

The diagnostic next used assertion strings from pinned official `patternsleuth/src/resolvers/unreal/guobject_array.rs`, exact string references, PE exception entries and chained unwind records. It recovered these roots:

| Research lead | Root RVA | Observed scope |
| --- | --- | --- |
| Signature match and allocation-like initialization | `0x2B861C0` | Six chained chunks, 2,723 bytes through `0x2B86C63` exclusive |
| Object-array shutdown assertion | `0x2B1FDA0` | Two references to one UTF-16 string; nine chunks, 1,526 bytes |
| GC-pool allocation assertion | `0x2B83670` | One reference; four chunks, 691 bytes |
| Wrong-object-index removal assertion | `0x2B83B60` | One reference; five chunks, 296 bytes |

The concurrency assertion used by another resolver alternative was absent. Function names supplied by upstream are research leads; compiler inlining means a recovered root need not be a standalone callable with that name. None was called. Bounded MSVC disassembly started at measured root boundaries and followed all same-root unwind chunks within an 8 KiB limit. This corrects the first short excerpt's midinstruction boundaries.

Initialization and shutdown independently access the same region around `0x6DD7230`. Initialization shows 64K chunk arithmetic, eight-byte chunk-pointer allocation, a pointer store at candidate-relative `+0x10` and count-related stores at `+0x20`/`+0x28`. Shutdown reads a count at `+0x24` and the pointer at `+0x10`. The follow-up below independently constrains the stride and separates slot extent from chunk capacity/count; live values, continued lifetime and safe traversal remain unverified.

## Item layout and slot extent

Finding `ARRAY-STATIC-02` constrains the indexing required by an eventual read-only observer. Confidence is **HYPOTHESIS for the native binding**; the operands and allocation sizes below are exact-file static observations. Inputs are the executable fingerprint above and the official loader/Unreal pins in [the candidate profile](upstream-startup-candidate-profile.json). Nothing was read from a live object or called.

The allocation root calls `0x2B8D720` at `0x2B83727` and `0x2B83785`, passing candidate-relative `+0x10` as its first argument. Following that measured direct target through its chained unwind records retained a 219-byte scope. The function requests `0x180008` bytes, writes a `0x10000` count prefix, advances eight bytes, and zeroes 65,536 items while advancing `0x18` bytes per item. Independently, the allocation and removal roots calculate an item's address with quotient/remainder by 65,536, multiply the within-chunk index by three and scale by eight. The preallocation branch also multiplies capacity by `0x18`. Together these support a **24-byte item stride**, rather than importing the stock template's size.

| Candidate-relative field | Measured behavior | Interpretation still requiring runtime verification |
| --- | --- | --- |
| `+0x10` | Initialization publishes an eight-byte pointer; allocation/removal use it as a table of eight-byte chunk pointers | Chunk-pointer table |
| `+0x18` | Preallocation publishes the contiguous item allocation after its count prefix | Optional preallocated item storage |
| `+0x20` | Initialization stores chunk capacity shifted left 16 | Rounded item capacity |
| `+0x24` | Growth reads the old value, adds requested slots and returns the old value; allocation/removal use it as an upper bound | Slot extent, including holes |
| `+0x28` | Initialization stores chunk capacity and uses it to size the pointer table | Maximum chunk count |
| `+0x2C` | Growth indexes the chunk table with this value, publishes a pointer using compare/exchange and increments it only after successful publication; preallocation copies chunk capacity here | Published/allocated chunk count |

The `+0x24` value is **not a live-object count**. Removal clears an item without decreasing it and can append the retired index to a four-byte recycle collection at `+0x58`/`+0x60`; allocation can pop the last recycled index. These native paths assume valid indexes and do not establish a safe check for arbitrary negative input.

Allocation writes the incoming object pointer at item `+0`, operates atomically on a 32-bit word at item `+8`, and writes the chosen index at object `+0x0C`. Removal reads that object index, reconstructs the item and compares its object pointer with the incoming object before clearing it. This supports the pointer, flags-word and internal-index interpretations. Clearing item `+0x0C` and `+0x10` does **not** identify those fields as ClusterRootIndex and SerialNumber. Upstream's labels and PendingKill semantics remain separate unverified assumptions. A process-lifetime object serial, even if later established, would not prove a persistent native save-instance identity.

Pinned `UObjectArray.cpp:272-330` and `UObjectGlobals.cpp:882-910` consume these source layouts. Ordinary startup enumeration visits all `NumChunks * 65536` items, including the final chunk, rather than capping at `NumElements`; its callback index is chunk-local. Qualification must therefore establish allocated/initialized chunk coverage and any global-index conversion, as well as live counter consistency. Generated accessors and enumeration cache their offsets/stride on first access. This research neither runs that enumeration nor changes those caches.

## Listener layout needs independent qualification

Shutdown iterates two eight-byte pointer collections, calling a virtual method at `+0x10` on each element and checking that the collection was emptied. Pointer/count pairs occur at candidate-relative `+0x78`/`+0x80` and `+0x68`/`+0x70`.

The allocation assertion's root independently iterates the `+0x68` collection after assigning an object index, passing object/index values to a virtual operation at `+0x8`. This supports identifying that collection as create listeners. A separate function at root `0x2B83930` appends an incoming pointer to the `+0x78` collection, adjusting its count/capacity at `+0x80`/`+0x84`; root `0x2B839A0` searches and removes a pointer from it. Both use synchronization storage at `+0x88`. These are consistent with the other collection being delete listeners, but the interface, allocator and synchronization semantics still require runtime confirmation.

The official `assets/MemberVarLayoutTemplates/MemberVariableLayout_4_26_Template.ini` instead specifies create/delete listener positions `0xE0`/`0xF0`. This is a material discrepancy: stock template values cannot establish this retail layout. It is not authorization to install the newly measured candidates automatically.

Setting `bUseUObjectArrayCache=false` does **not** avoid all listener writes. Pinned `deps/first/Unreal/src/UnrealInitializer.cpp:333` unconditionally adds `FShutdownDeleteListener` after the cache-controlled block. Its StaticConstructObject postcallback at `351-379` is also unconditional and accesses object flags/class/index. `UObjectArray.cpp:381-386` accesses the delete collection and appends to it. Address setup at `344-346` only casts/stores the candidate address, without validation. Generated member accessors also cache offsets on first access. Any future verified profile must establish its layout before those accessors run; changing configuration afterwards is insufficient.

## Listener allocation and synchronization discrepancy

Finding `LISTENER-STATIC-02` identifies a prerequisite beyond field offsets. A 148-byte recovered root at `0x2B18200` uses the delete collection and holds the same synchronization storage as registration/removal. The on-disk import table identifies IAT RVAs `0x55AD850` and `0x55AD858` as `KERNEL32.dll!EnterCriticalSection` and `LeaveCriticalSection`. Notification iterates backwards, calls listener vtable `+8` with listener/object/object-index arguments, and then releases the critical section. Shutdown calls vtable `+0x10` under that section and subsequently expects the collection to be empty. This agrees with the shape of upstream's polymorphic listener interface, but does not qualify its live ABI, mutation rules, callback thread or lifetime.

Following measured direct calls retained growth root `0xE47F00` (192 bytes) and removal's shrink root `0xE6BA70` (202 bytes). Both use eight-byte pointer elements and signed 32-bit count/capacity fields. They load the allocator singleton candidate at RVA `0x706B500`, call vtable `+0x38` with a byte count and zero alignment, and call `+0x20` with the old pointer, capacity multiplied by eight and zero alignment. The shrink path may invoke the latter with a zero size; it is not a direct observation of a Free call. The existing shutdown scope separately calls allocator vtable `+0x30`. These agree with the pinned generated 4.26 source's QuantizeSize/Realloc/Free positions and argument shapes. Concrete allocator identity, allocation provenance and calling safety remain unverified; agreement does not establish the retail engine version.

Upstream `UObjectArray.cpp:379-391` performs Contains/Append and RemoveSingleSwap **without the measured native lock**. Its lock/unlock methods at `442-450` are empty TODOs. Append constructs a temporary engine-allocated array even if the destination has spare capacity, and removal permits shrinking. Capacity can be published before Realloc returns; a callback or failure cannot be treated as a harmless pointer-only insertion. Thus **correcting listener offsets alone does not resolve synchronization or allocator safety**. No speculative lock address or native registration function was installed.

## Evidence retained and remaining gate

All raw artifacts are ignored under `out/local/upstream-startup-01`; no executable, save, process-memory dump or generated SDK was committed. No game process or game object was accessed for this research.

| Local artifact | SHA-256 |
| --- | --- |
| `shutdown_scan.py` | `CABFD67151A007237F48CB9EB528DB839D93D1D6F63AD15F069103785352CD06` |
| `shutdown-scan.json` | `150C9AAD18F2B271F64560651FF0E35BDFDCC45C691A945F505BCE9F3E949718` |
| `unwind-chunks.jsonl` | `32CEE6D613560C9495681C5A813547CD3344F8FB22D67567870C2EADEF3EA8FE` |
| `listener-xrefs.json` | `A28D6E6A2D7426902E2FA77D68322D99118FFD2798D7ED60DCF117D55C0CD9DB` |
| `delete-listener-candidates-disasm.txt` | `02CBD6FB086590A9A5D3FD87B70B591373AE92378BEED18A45E81DC59E592C01` |
| `item_layout_scan.py` | `F66230E9D3B834B73279047036B75C59812BD31AF7F5E3FAA6CF164594E670FD` |
| `item-layout/report.json` | `8D16056C3E7E82B0E4033A19BAF1819F861C44E159490CB3A0C3CF0DDEA125BA` |
| `item-layout/chunk-item-growth-disasm.txt` | `C8B4176101C89D42BAA7F5F6A37748315ACDFD48ED83090476AA8EDF51BCA171` |
| `item-layout/listener-pointer-growth-disasm.txt` | `FFBCBE2673047F554690D311F4FFB7199902B14FEE71A173A9F9F6C80EFB359B` |
| `item-layout/delete-notification-disasm.txt` | `D6CE38259C72F28095B1A426AAF3C51E50BA917FB6EEAF658316CCEE1B420421` |
| `listener_shrink_scan.py` | `5461D5728CDFF646C9BE3B9D8702286E067B092385D8BEC0C2DEB83DF3714FDC` |
| `listener-shrink/report.json` | `48D9AF50033D2125DB6E5AD81CC14A14DC4F83DB5337FEFCDC4B95543877FC28` |
| `listener-shrink/listener-pointer-shrink-disasm.txt` | `4865E4E43C0B47FB1B5E22D31F0D628AD581315BCE774CB07F6EE5656782B641` |

The item/layout follow-up retained three scopes totaling 559 bytes. The separate shrink follow-up retained 202 bytes and rechecked the exact executable hash before disassembly; it exited 0. The scanner limits each same-root scope to 8 KiB, aggregate scope to 16 KiB, unwind depth to 64 and each dumpbin call to 30 seconds. Hashes of the new reports, scanner and raw clips were rechecked against current files. CMake/CTest were not rerun for this documentation/offline-diagnostic capability; no C++ source or build profile changed. Prior candidate helper tests remain those recorded in the candidate profile, and no native runtime/save test is claimed here.

The findings constrain array indexing and expose a concrete synchronization gate. They do not establish game-thread entry, persistent object validity, create/delete listener ABI, allocator safety, clean teardown, native save inventory or loaded-save identity. The next useful runtime experiment must isolate callback qualification **before ordinary FName/object traversal and listener registration**; the current deadline candidate does not implement that mode. Resolve its source/callback prerequisites and define a controlled read-only verification experiment before adopting any native profile. Ordinary saves remain unflagged, and native creation/admission stays disabled.
