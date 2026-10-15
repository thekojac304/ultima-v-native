# v0.5.0 RC1 integrated baseline

Gate 0 and Gate 1: **PASS**, 2026-10-15 (America/New_York). No optimization was applied before these measurements.

## Provenance and preservation

- RC branch `release/v0.5.0-rc1`, worktree `C:\Dev\ultima-v-native-rc1`.
- Baseline source: `9c98054782eb9b62eefbb70718535991e0dede98` (`windows/look-enhanced-polish`). All gameplay/application sources were unchanged during baseline builds. Diagnostic tools and RC notes were untracked.
- `main`, `windows/initial-port`, SPP1 `fc4a63aa`, SPP2 `bf3480e3`, Original DOS `00ae0410`, and packaged LOOK-POLISH1 `5b7f8bbc` are ancestors (`git merge-base --is-ancestor`, all exit 0).
- The separate `windows/original-dos-shops` history is not an ancestor: its record `00e3bd22` is patch-equivalent to inherited `98e26f43` (`git log --cherry-mark`). The inherited interaction branch includes the actual shop transcript implementation `07edc45b`, DOS screen `ccb09116`, DOS pixel tests `f5361508`, and scenario follow-up `c6a8c6a8`. No merge or duplicated feature work is needed.
- All 15 pre-existing checkout HEADs and statuses matched the initial snapshot after worktree creation. Four main-checkout evidence files, numerous older Windows logs, and the existing `uvw-dos2-base` test edits remain untouched. Manifest: `v050-rc1-evidence/preexisting-worktrees.json`.
- Private inputs copied into RC: packs, game/assets, map fixtures, loot names and generated corpora/includes. `original` and root `node_modules` are read-only-use junctions to existing inputs. No proprietary input is staged or packaged.

## Build/test commands and results

Evidence directory (ignored, retained locally): `native/targets/tdeck/dist/v050-rc1-evidence/`.

| Target | Command / configuration | Result |
|---|---|---|
| Native host | GCC 16.1.0/w64devkit, CMake/Ninja, Release, developer tools ON; `cmake -S native/core -B native/core/build-rc1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENU5_ENABLE_DEVELOPER_TOOLS=ON`; build `-j 4`; `ctest --test-dir native/core/build-rc1 -j 1 --output-on-failure` | 210 tests exercised. Initial 190 PASS / 20 checkout-format failures; all 20 PASS after exact LF normalization, with no semantic source/test changes (`baseline-host-lf-verified.log`). |
| Windows | `native/targets/windows/tools/build-windows.ps1 -BuildDir <RC>/native/targets/windows/build-rc1 -Test`, Release, MSVC 19.51.36260.0/static runtime, SDK selected by vcvars | **38/38 PASS**, including Original/Enhanced, SPP1/SPP2, Look, shop prices, DOS panels, scaling, save/import/export and audio; `baseline-windows-build-test.log`. Embedded Git `9c98054782eb`. |
| T-Deck | ESP-IDF 6.1, Python 3.14.5, Xtensa GCC 15.2.0 (esp-15.2.0_20251204), CMake 4.0.3/Ninja 1.12.1; export IDF, `idf.py --no-ccache -B build-rc1-baseline reconfigure`, `ninja -C build-rc1-baseline -j 4` | **PASS**, including the post-build app budget; project `-Wall -Wextra -Wconversion -Werror -fstack-usage`; `baseline-firmware-build.log`. |

SDL 3.4.16 was extracted into the new build from the existing official release archive after verifying SHA-256 `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`. No old binary was relabeled.

### Diagnosed setup issues

1. Initial host compilation lacked the private `build-shops/assets.inc`; copying the generated include (and companion private includes) resolved it.
2. Fresh Git checkout used `core.autocrlf=true`. Exact-output generator drift checks and an audio source scan require LF: local CRLF normalized to the existing LF Git blob had identical normalized content. The first checkout-index attempt retained cached files; direct byte inspection proved that, then direct RC-only LF normalization resolved all 20 failures. Added explicit `.gitattributes` prevents recurrence; no test expectation was weakened.
3. The firmware itself built successfully; its first JSON-report invocation used one excess `..` in a relative path. The corrected root-relative invocation produced `baseline-firmware.json` without rebuilding or changing the image.
4. The sandbox launcher fails before process creation (HRESULT 0x80070020); approved execution fallback was used. This is separate from project build results.

## Firmware baseline

`build-rc1-baseline/openu5_tdeck.bin`: **1,096,432 bytes** (`0x10baf0`), matching the historical LOOK-POLISH1 image size with the same sdkconfig SHA-256 `b0425893c6a5c45e080721617ace20fbbad412bc8b3a601d1be3ce1f4bf8b382`.

| Quantity | Bytes |
|---|---:|
| flash `.text` | 758,330 |
| flash `.rodata` | 238,524 |
| internal `.data` | 22,091 |
| internal `.bss` | 54,096 |
| IRAM code + vectors (before alignment tail) | 77,067 |
| app partition | 1,310,720 |
| partition free | 214,288 (16.3%) |
| Launcher allocation | 1,114,112 (1,088 KiB) |
| free within Launcher allocation | 17,680 |

See `baseline-firmware-size.log` and `baseline-firmware.json` for complete memory regions/sections and compiler stack frames. No static PSRAM BSS/data allocation: PSRAM is a runtime heap; `.ext_ram.dummy` reserves an address range and is not application data. Heap free bytes and real stack high-water marks require hardware and are not inferred from ELF output.

Main task stack remains 24,576 bytes, audio task 6,144 bytes. Largest project compiler frame is DeviceSmokeTests::run 5,920 bytes; PC export 5,392; dungeon gem view 4,928; render 2,720. These are individual frame estimates, not complete call-chain bounds or fresh hardware measurements.

## Next gate

Measure linked Windows-only candidates; freeze the pre-optimization probe traces before changing the capability boundary. Preserve shared gameplay/UI, the default handheld profile, saves, diagnostics and developer features. SPP1 hardware PASS is user-reported; SPP2 and this RC remain pending physical acceptance.
