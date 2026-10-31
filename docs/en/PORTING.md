# Porting game addresses to new Fallout 4 versions

## Why
The client hooks and calls engine functions through addresses. Next-Gen (from 1.10.980) was recompiled, the
Address Library uses a **new ID numbering** there, and new patches (such as 1.11.240) often have no Address
Library for a long time. That is why we port directly from the exe files ourselves.

## How the client loads addresses (patch 0002, `Code/client/VersionDb.h`)
- 1.10.163: `Data\F4SE\Plugins\version-1-10-163-0.bin` (Address Library).
- Other versions: `Data\F4SE\Plugins\FalloutTogether\offsets-<a-b-c-d>.bin`, layout like the Old-Gen library
  (`u64 count`, then `u64 ID, u64 RVA`), but keyed by **Old-Gen ID**.
- Every `VersionDbPtr` registers its ID. If IDs are missing, the client writes `logs\missing-offsets-<version>.txt`
  and stops (unless `TP_ALLOW_MISSING_OFFSETS=1`).

## Steps for a new Fallout patch
```powershell
# 1. once: provide a locally analysable 1.10.163 exe as Fallout4.analysis.exe (not part of the project)
# 2. optional: a Next-Gen intermediate version with Address Library (e.g. 1.11.191) as a second path
python -I Tools\port\pipeline.py --ids data\ids-1.10.163.csv --og-exe Fallout4.analysis.exe `
    --new "<Steam>\Fallout 4\Fallout4.exe" --mid-exe Fallout4-1.11.191.exe --mid-anchors anchors-163-191-func.csv `
    --version 1-11-240-0 --install "<test copy>"
```
Duration: seconds. Result: `offsets-<version>.csv` + `.bin` in the game folder.

## Ghidra steps for difficult addresses
For addresses that `graphport.py` does not find. Needs `FTR_WORK`, `FTR_GHIDRA` and `FTR_JAVA_HOME` ([ENVIRONMENT.md](ENVIRONMENT.md)) as well as `Fallout4.analysis.exe` in `%FTR_WORK%\exe-1.10.163` and `Fallout4.exe` in `%FTR_WORK%\exe-1.11.240`.

```powershell
Tools\port\ghidra\run-dump.ps1 old     # listing of all functions -> %FTR_WORK%\ghidra\dump\old.txt
Tools\port\ghidra\run-dump.ps1 new
python Tools\port\ghidra\lst.py        # list leaf functions without .pdata
Tools\port\ghidra\run-extra.ps1 old <in> <out>   # fetch their listings, same for new
python Tools\port\ghidra\prop.py       # propagate the mapping -> ext.pkl
python Tools\port\ghidra\final.py      # rate open addresses -> final.pkl
python Tools\port\ghidra\report.py     # data\manual-ghidra-*.csv and report
```
The starting point is `data/out/map-240-plus.csv`, the old-to-new mapping. `run-decomp.ps1 new|old <list> <output>` decompiles single functions, `cmpf.py old:new` compares two functions.
For another Fallout version the version numbers in `fdis.py`, `run-dump.ps1` and `ctx.py` must be adjusted.

Limit of this repository: the analysis needs a readable 1.10.163 exe, which is not included here. Finished ported addresses for 1.11.240 are in `data/final-1-11-240-0.csv`.

## Tools (`Tools/port`, Python standard library only)
| Script | Purpose |
|---|---|
| `inventory.py` | all IDs in the code with real ID and 1.10.163 address → `data/ids-1.10.163.csv` |
| `build_map.py` | OG→NG ID pairs from CommonLibF4 (`RelocationID(og, ng)`) → `data/og_to_ng.csv` |
| `sigport.py` | byte signature at the function start, rel32 targets as wildcards, search only at `.pdata` function starts, unique hit required |
| `graphport.py` | main method, see below |
| `make_offsets.py` | builds `offsets-<version>.bin` from NG Address Library + mapping and/or CSV `og_id,rva` |
| `validate.py` | measures hit and error rate against CommonLibF4 + NG Address Library |
| `pipeline.py` | everything together, two paths, discards contradictions |

### graphport.py: method
1. **String anchors**: functions that alone load a given C string through a RIP-relative address
   (same text in both versions). About 6,300 anchors.
2. Optional: signature hits (`--sig`), confirmed pairs (`--anchors`), VTable slots (`--vtable-anchors`).
3. **Propagation** (12 rounds): for mapped function pairs, call targets (`call rel32`) and
   RIP data accesses are paired in order; with unequal length only gaps of equal length, using known pairs as
   support points. Votes are collected, ambiguity is discarded, a new function belongs to only one old one.
4. **Gap filling** (`--gaps N`): between two neighbouring mappings with the same number of functions
   in between (≤ N), pair by order (linker order is often preserved).
5. **VTables through RTTI**: CompleteObjectLocator → TypeDescriptor name + offset → exact mapping.
6. Locations inside functions (hook points): search 12 bytes from that location in the mapped new function.

## Validation (1.10.163 → 1.11.191)
Truth: CommonLibF4 pairs (OG ID, NG ID) + `version-1-11-191-0.bin` → 2,066 pairs. Of those 896 are VTables,
98 are function starts.
- Without anchors: 106 correct, 1 wrong.
- Cross-validation (half as anchors, half as test), functions only: 49 correct, **0 wrong**, 49 open
  (`--gaps 4 --vtable-anchors`). `--gaps 16` gave 2 errors, so the default stays 4.
- VTables: RTTI contradicts CommonLibF4 in many cases; a cross-check against the names from
  alandtse/fallout_vr_address_library confirms RTTI. **CommonLibF4-NG VTable IDs are partly wrong.**

## Result for 1.11.240
- Required (Fallout build, without `Games/Skyrim`): 133 IDs.
- Found: 80 (`data/offsets-1-11-240-0.csv`). Path A (direct) and path B (through 1.11.191) agree on all
  77 shared mappings.
- Open: 53 (`data/open-1-11-240-0.csv`): 46 functions, 7 data.

## Open addresses: ideas
- Similarity measure per function: set of mapped callers/callees/data targets/strings/immediates,
  candidates from the neighbourhood of mapped functions, Jaccard with distance to the runner-up.
- Global variables: accesses from already mapped functions at the same place (instruction order).
- F4SE-NG source (`f4se/Game*.cpp`) contains named RVAs for 1.11.x; link them with names from
  `fo4_database.csv` (alandtse/fallout_vr_address_library).
- By hand: Ghidra/IDA with both exe files, the report `*.report.csv` shows old RVA and name.
- Measure every new heuristic with `validate.py` against 1.11.191 first.

## Important about the Old-Gen Address Library format
`version-1-10-163-0.bin` = `u64 count`, then `u64 ID, u64 RVA`. The original Fallout Together code read
`(RVA, ID)` from byte 0, see [FINDINGS.md](FINDINGS.md).

## Addendum: fuzzport.py and sitevote.py
`graphport.py --dump-map` writes all pairs. `fuzzport.py` (neighbourhood similarity) and `sitevote.py` (voting at use sites) build on it.
Holdout with 2,000 hidden functions: fuzzport strict 670 correct, 4 wrong; loose 1,098 correct, 10 wrong. sitevote: 204 correct, 0 wrong; data 357 correct, 6 wrong.
Result: 11 more addresses, status 97 of 139. These are single-stage hits without a second path, so they rate as less certain. Further runs bring nothing more, the rest needs manual work in the disassembler.
