# Ultima V Native v0.5.0 RC1 — integration and acceptance handoff

**Unpublished engineering candidate. Physical RC testing pending.** Earlier SPP1 physical PASS and acceptance of Windows Original/LOOK-POLISH1 are user-reported. This record does not certify the new RC on hardware.

## Source and repository provenance

Branch: `release/v0.5.0-rc1`; isolated worktree: `C:\Dev\ultima-v-native-rc1`. Base: `9c98054782eb9b62eefbb70718535991e0dede98`, the verified latest Windows development HEAD. The original main and all 14 other existing worktrees/builds were preserved; original statuses, untracked evidence and the existing DOS-base test edits are inventoried in [the initial manifest](v050-rc1-evidence/preexisting-worktrees.json).

Ancestry checks passed for main, the initial Windows port, SPP1 `fc4a63aa`, SPP2 `bf3480e3`, Original DOS interaction `00ae0410` and packaged LOOK-POLISH1 `5b7f8bbc`. The parallel shop branch's record is patch-equivalent to inherited `98e26f43`; the actual transcript implementation, native DOS screen, pixel tests and scenario follow-up are already inherited. No feature merge, duplicate implementation, main update, rebase, reset, push, publication or device flash was performed.

Incremental implementation checkpoints: `aeaa8a34` (baseline/fixtures/diagnostics), `1c4b65b9` (handheld capability isolation), `c1453fc` (RC identity and release verification). The final **full source hash is in the external artifact manifest** and in both binaries' embedded 12-character Git identity. The manifest is generated after the clean source commit and contains full artifact hashes; keeping it outside tracked source avoids circular commit/checksum references.

## Integrated behavior retained

- **Windows Original:** DOS 320×200 composition, party/status frame, original equipment/item lists, core DOS shop transcripts, original keyboard/prompts/spell entry/console, Framed / Whole pixels / Fit. The accepted interface was not redesigned.
- **Windows Enhanced:** 320×240 handheld presentation, contextual menus, aligned `gp` prices, owned Ready/Use counts, character/magic menus, saves, fullscreen/windowed handling, music/SFX.
- **Shared:** SPP1 and SPP2 timing, approach/encounter/music behavior, combat, magic, inventory, shops, quests and DOS saves; D-117's Look post-to-sign chase and LOOK-POLISH1's shared handheld row improvements. No save field or gameplay rule changed in RC-specific work.
- **T-Deck:** existing handheld interaction, trackball/keyboard, seven settings rows, diagnostics/developer tools, difficulty/cheats, shared UI and SD resources retained. This RC uses `FW 0.5.0-rc1`; old alpha firmware versions remain historical records.

## Baseline and actual validation

The [baseline record](V050_RC1_BASELINE.md) gives exact commands/toolchains, source and configuration hashes, initial failures and their diagnoses. Gate 1 passed before optimization. Host baseline exercised 210 tests: initial 190 PASS plus 20 LF-sensitive checks resolved and rerun PASS with unchanged semantics/test expectations. Windows baseline passed 38/38. Fresh ESP-IDF 6.1 baseline app: 1,096,432 bytes.

The implementation passed the full serial **212/212 host tests** (210 inherited plus dynamic/fixed capability contracts), **38/38 Windows checks**, **11/11 relevant TypeScript tests** (`look-signpost`, `spp2-talk-refusal`), and **5/5 release-tooling regression methods** (four provenance checks plus the scenario-option contract). Full final suites run again after the final commit; their authoritative counts, source identity and log hashes are recorded in `native/targets/tdeck/dist/v050-rc1-manifest.json`.

The final evidence directory is `native/targets/tdeck/dist/v050-rc1-evidence/` (ignored, retained locally). Important files:

| Evidence | Scope |
|---|---|
| `final-source-commit.txt` | Full clean revision used for both builds |
| `final-host-build.log`, `final-host-ctest.log` | Complete native build, serial host suite including preservation, saves, audio, real-data parity and generator drift |
| `final-windows-build-test.log` | Full MSVC build/package and 38 tests: Original/Enhanced, native DOS panels, three scaling modes/fullscreen, SPP1/SPP2, Look, shop prices, saves/import/export and audio |
| `final-typescript.log` | Relevant reference preservation checks, one worker; 11 tests |
| `final-tooling-regression.log` | Exact RC version/clean provenance plus smoke options; five methods |
| `final-firmware-build.log`, `final-firmware.json`, `final-firmware-size-diff.log` | ESP-IDF build, warnings-as-errors, budget, ELF sections and compiler frame evidence |
| `final-differential/differential-summary.json` | Strict frozen-baseline comparisons and positive control |
| `final-boundary-mutations.log` | Eight platform-boundary mutants killed, exact source restored |
| `final-release-tooling-mutations.log` | Version-prefix and missing-developer-opt-in mutants killed; restored verifier/runner PASS |
| `package-smoke/package-smoke-summary.json` | Fresh extraction, portable launch, missing-data refusal, bundled data prep, real executable scenarios |

The broader historical TypeScript run has 116 environment-bound failures (LOOK-POLISH1 record), not a clean full-reference PASS. This RC changes no TypeScript gameplay code; the requested relevant TypeScript route was used, and all 11 selected checks passed. No historical failure set was silently called green or disabled.

## Firmware-size audit and optimization

Same ESP-IDF 6.1, Xtensa GCC 15.2.0, sdkconfig, warnings and optimization settings before/after. Windows `board_dos_layout.cpp`, desktop input adapters and desktop helpers were already excluded by target source lists. They were not counted as newly recovered bytes.

| Contribution | Classification / decision |
|---|---|
| DOS list-panel composition/drawing, DOS prompt/typed-spell branches, original-only fallback names | Windows-only presentation reachable through dynamic profile callbacks; exclude only when compiling the fixed handheld target |
| Platform key/storage wording, settings, shared handheld menus/rows | Shared UI semantics; retain |
| Timing, combat, magic, Look, inventory, shops, persistence/quests | Shared gameplay; retain |
| Board/device input/audio/SD, diagnostics/developer tools | T-Deck features; retain |
| Core shop transcript machinery still expressed through shared shop services | Retained; extracting more would require a larger refactor with poor value for this RC |

One `OPENU5_FIXED_HANDHELD_PROFILE=1` definition on the firmware component fixes its existing typed-spell / Quit-save / DOS-interface / DOS-screen / DOS-text defaults in one header. Ordinary host/Windows builds keep dynamic callbacks. The Board's capability constant drops its unreachable DOS drawing branch. Normal compiler folding and existing section garbage collection do the rest. Profile layout, shared rules, warnings and saves are unchanged.

Measured clean optimization checkpoint:

| Metric | Integrated baseline | Isolated handheld | Change |
|---|---:|---:|---:|
| App image | 1,096,432 B | 1,075,088 B | **−21,344 B** |
| Flash `.text` | 758,330 | 738,518 | −19,812 |
| Flash `.rodata` | 238,524 | 236,988 | −1,536 |
| Internal `.data` | 22,091 | 22,091 | 0 |
| Internal `.bss` | 54,096 | 54,096 | 0 |
| IRAM code + vectors, before alignment tail | 77,067 | 77,067 | 0 |
| Free in 1,310,720-byte app partition | 214,288 (16.3%) | 235,632 (18.0%) | +21,344 |
| Launcher allocation | 1,114,112 (1,088 KiB) | 1,114,112 | 0 |
| Free inside Launcher allocation | 17,680 | 39,024 | +21,344 |

The mapped section total falls 21,348 bytes; image padding explains the four-byte difference from the file saving. Main attribution: `alpha_runtime` text −10,616, `ui_session` −6,551, Board −1,781, platform profile −154; additional branch/string folding accounts for the rest. The map no longer links `compose_dos_panel` (3,197-byte baseline symbol), `draw_dos_panel` (1,140 bytes), their row helpers, or DOS getdir/modal helpers. Small ±4-byte per-object differences are linker/code alignment, not inferred gameplay changes.

A deliberately broad name-equality audit also noticed `ShopRun::buy_row_name` (54 bytes) no longer has an out-of-line symbol. Inspection of `shop_orchestration.cpp` shows that constant-null DOS text lookup makes this tiny helper inlineable; its normal equipment-name behavior remains. All shop differential walks and parity tests agree. `world_look` and `AlphaRuntime::command` remain linked. No unexplained removed gameplay path was accepted.

Static RAM and IRAM are unchanged. No static PSRAM BSS/data is allocated; PSRAM is the existing runtime heap. The ELF `.ext_ram.dummy` reservation is not application PSRAM usage. Main/audio task budgets remain 24,576 / 6,144 bytes. Largest individual compiler frame remains 5,920 bytes (smoke tests), followed by PC export 5,392 and dungeon gem view 4,928. Nine retained frames shrink; none grows (Board 2,656→2,640; modal handling 288→208; the now-discarded DOS composition's object frame 1,152→32). These estimates are not whole-call-chain bounds or new hardware high-water measurements. Observe existing heap/stack diagnostics during acceptance.

**Stopping rule applied:** straightforward isolation is complete; no further compiler tricks, feature removal or speculative gameplay refactor was attempted. Final RC-version image/section measurements and checksums are read from the actual packages by the external manifest verifier.

## Differential, mutation and independent review

The exact pre-optimization probe executable and traces were frozen before changing the platform boundary. Baseline and candidate use the same real T-Deck runtime, Board, fake bus, packs and GCC configuration. Paced and unpaced runs each cover **1,065 walks / 8,514 snapshots** (7,449 subsequent input steps plus each walk's initial snapshot). Raw files are byte-identical: panel pixels, transcript/channel/flags, state, prompts, arena, RNG, turns/clock, audio calls and saved GAM hashes agree at every observation. No difference classification was needed. The DOS positive control differs in **1,054 walks**, proving sensitivity.

Unpaced SHA-256: `7909072837036911266e5a34bb25f37950aecaf28017cfd71afb63c2cf082526`; paced: `753e3933b848f4ea48053a935b3faa78138230d07a255d5d574e5bb76cfb2a8a`. These are host-executed T-Deck code traces, not physical-device execution.

Six compilable capability mutants and two actual-command-list wiring mutants were killed (8/8). The ordinary dynamic profile remains tested separately. Two additional release-tooling mutants (exact-version prefix acceptance and missing developer opt-in) were killed by the regression suite. Every mutated source was restored and the restored tests passed; compile failures were never counted as kills. Unrelated historical mutation suites were not rerun.

[Independent review and resolutions](v050-rc1-evidence/independent-review.md): a separate reviewer found two RC-specific defects (README UTF-8 corruption and manifest version-prefix acceptance), both fixed and rechecked. No unresolved serious runtime, capability, preservation, packaging or licensing finding remains. A subsequent fresh-ZIP smoke found a runner setup defect: the gameplay scripts require the same `--developer` opt-in CTest supplies. The helper now carries explicit scenario requirements; a regression test checks them against the authoritative CMake declarations, and a corrected complete extraction smoke passed before rebuilding from the new final source. The failed attempt and its artifacts remain intact under `v050-rc1-evidence/attempt-18e464e3a928/`. No runtime source or accepted interface changed.

## Packages, provenance and smoke

Both newly built artifacts must identify the same clean final commit. The final verifier rejects dirty source, mismatched package/build bytes, incorrect embedded identities/versions, non-x64 Windows PE, ZIP extras/duplicates/CRC errors, malformed ESP image/header/checksum/SHA, incomplete suites and stale package-smoke identity.

| Artifact | Exact path |
|---|---|
| Windows ZIP | `C:\Dev\ultima-v-native-rc1\native\targets\windows\dist\UltimaV-Native-v0.5.0-rc1-Windows-x64.zip` |
| T-Deck Launcher BIN | `C:\Dev\ultima-v-native-rc1\native\targets\tdeck\dist\UltimaV-Native-v0.5.0-rc1-TDeck-Launcher.bin` |
| Full commit, sizes, SHA-256, toolchains/dependencies, final metrics/results | `C:\Dev\ultima-v-native-rc1\native\targets\tdeck\dist\v050-rc1-manifest.json` |

The Windows whitelist includes executable, SDL3 runtime, player README/controls, data-preparation command/bundle, GPL/NOTICE and third-party licenses, and BUILD-INFO. Static C/C++ runtime needs no separate VC++ redistributable. SDL source archive SHA-256 was verified against the project's pinned 3.4.16 release; source override use is truthfully recorded in BUILD-INFO. No proprietary Ultima V assets are in either package.

The established Launcher packager validates the bare ESP32-S3 app image; the requested RC filename is a byte-identical copy of that new output. It is not a merged image or direct-flash instruction. Existing Alpha 4/SPP1 SD resources and save formats remain compatible. Package smoke extracts to a new folder, checks missing assets by exit/log under offscreen SDL, prepares the user's private packs via the bundled tool, verifies all three hashes, and runs portable boot, Look in both Windows modes, Enhanced prices and Original DOS panels. Runtime launch uses only the package and Windows system directories on PATH. Pixel/assertion scenarios and earlier user acceptance support the presentation result; no new human Windows play session is claimed.

## Hardware handoff and remaining work

Use [the focused 20–30 minute checklist](V050_RC1_HARDWARE_CHECKLIST.md), including exact Launcher installation, manifest Git verification, backups, rollback, SPP2 timed Mix/approach/combat-music/Talk/save checks, SPP1 spot-checks, D-117, and menus/input/audio/performance. Known-good SPP1 fallback: `C:\Dev\spp1-hw-package\OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin`, embedded `Git 5ba2fa46b74f`, SHA-256 `1aeecb70dec68141122e4aa88a9ce140cad87a617ff5668ec3eb8fa1d8b123c2`.

**Release blocker: new RC physical acceptance remains pending.** SPP1 PASS is the user's earlier report; SPP2 and new shared Look/rows/optimized firmware still require the checklist. Real heap/stack high-water margins and device audio/display/performance must be observed there.

Existing preservation issues remain deferred with their established evidence: D-116/D-107(2) camp/bed trailing turns, D-115/D-107(1) the town conversation turn, D-106 Rel Hur direction/wind, D-104 remaining one-line shop rules, and D-109 TypeScript dungeon Look/getdir rules. SPP2 saves during a timed hold intentionally save pre-settlement state; the binary-version caveat and 64-step/off-map Look guards remain documented. The handheld font's `g` shape and narrow long-row margin remain existing presentation limitations. This RC neither resolves nor hides those historical items.

After hardware PASS: retain the checklist, identity and logs; review any reported issue before further changes. Any code fix requires a new clean revision, rebuilt matching candidates and affected checks. Promotion, main integration, tags, pushing and publication require a separate explicit instruction; no public release was performed here.
