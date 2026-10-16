> Acceptance update, 2026-10-15 (America/New_York): the user personally tested final RC2 and reports everything works correctly on T-Deck. This is user-reported hardware PASS for source `355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9`, firmware SHA-256 `62f90df106132ab08c916b245c44cbf91cbb93d6e97f02c0ee33ef8ee7018ed9`, 1,075,504 bytes, embedded `0.5.0-rc2` / `355d4a1e564c`. No serial logs or photographs were supplied. The hardware gate is satisfied. [Final release record](V050_RELEASE_RECORD.md). Pending statements below describe the earlier engineering handoff.

# Ultima V Native v0.5.0 final UI polish

This is an unpublished RC2 candidate. Hardware acceptance is pending. Earlier user acceptance of Windows Original and the SPP1 handheld changes does not validate this candidate.

## Source and preservation

Branch `release/v0.5.0-ui-polish`, worktree `C:\Dev\U5Native\worktrees\ultima-v-native-ui-polish`, based on verified clean RC1 `a96656b2330097c18c52b78a8840d3749c0e9854`. The primary repository is `C:\Dev\U5Native\source`. All existing worktrees, untracked work, private assets, saves, recovery copies, and RC1 packages are retained. No merge, push, publication, reset, history rewrite, flash, or worktree deletion is part of this batch.

The final clean source hash and artifact SHA-256 values are in the external release manifest. Build/acceptance evidence is retained under `C:\Dev\U5Native\builds\ui-polish\evidence`. New packages are copied into `C:\Dev\U5Native\releases\v0.5.0-rc2\<full-source-hash>\ui-polish`. External manifests avoid circular source-commit/artifact-hash references.

## Introduction and View findings

`Return to the View` is an authentic original menu entry, not a modern diagnostic. `INTRO.OVL` invokes the `FONT.OVL` scene interpreter using four 19x4 maps at `MISCMAPS.DAT` offsets 704–1215 and the 655-byte script at 1216–1870. Its final RESTART loops automatically until input. The scenes are The Summoning, The Journey, The Arrival, and The Welcoming. They include the studio/mirror, moongates, Shadowlords, summoning beam, and dwelling choreography.

The existing native interpreter and renderer use that original script and original tiles. The player does not control the Avatar in this sequence. Animation advances automatically; any key (including Esc/Mic) returns to the main menu. Startup, main-menu idle timeout, and the R entry reach it. It is separate from the title's historical BRITISH.PTH rider path, and from the U entry's 21-scene Summoning narrative. The latter keeps extracted story text and illustration compositions, advances with input, and supports Esc/Mic cancellation. This batch does not invent sequences or claim new witness-exact DOS timing or layout fidelity.

Evidence: `re/notes/intro-attract-loop.md`, `re/notes/intro-summoning-scene-engine.md`, `docs/history/tdeck/ALPHA20_FRONTEND_HUD_SYSTEM_MENU_CORRECTION.md`, `native/core/src/intro_view.cpp`, and the frontend/runtime render paths. Earlier preservation findings remain unchanged. No small wiring defect was found; no content is hidden or removed. The inappropriate visible caption `Original FONT.OVL View script` becomes `Original Introduction` on both platforms.

## Player-facing changes

| Before | After |
| --- | --- |
| PC Save Transfer | DOS Save Transfer |
| Import the PC save into a slot / Import PC Save | Import DOS Save |
| Export a slot as a PC save / Export to PC | Export DOS Save |
| PC save status/errors/validation and export notes | DOS save terminology |
| Movement default / Trackball speed | Movement Default / Trackball Speed |
| Original FONT.OVL View script | Original Introduction |

Imported-save subtitles omit the redundant DOS prefix once a slot is shown, keeping long character/place/slot identities within the 50-cell subtitle. Missing-music help uses player wording instead of an internal npm command. The existing Ultima IV transfer placeholder now says "not yet available".

The original six menu choices remain in the same order with J/C/T/U/A/R hotkeys and numeric navigation. Settings and DOS Save Transfer retain S/P. A small vertical gap separates those Native utilities from the historical choices, including at Large text size. Selection remains reverse video and retained redraws use the same coordinates as full redraws.

Settings keep their existing row order and saved keys/defaults. Values align in a common column. A subtitle identifies the current row's Display, Controls, or Audio category, alongside the platform menu reminder. Gameplay difficulty and cheats remain in their existing System Menu pages; save/load remain their own pages. No tabs or new settings are added. The same labels, categories, and alignment apply to both title and in-game Settings.

Windows retains Interface, Display/fullscreen, and Scaling. Original continues to hide WASD Movement, Difficulty, and Cheats and follows the 1988 rules. T-Deck retains Brightness, Movement Default, Trackball Speed, Text / UI, SFX Volume, Music Volume, and Keyboard Backlight. Platform input code, hardware mappings, audio, SD handling, launcher behavior, and gameplay composition are unchanged.

## Shortcut policy

Windows **F10 is the primary System Menu shortcut**. Alt+M remains available. F1 remains unmapped; Ctrl+M keeps its music-mute function. The selected Settings entry on the title advertises `F10: System Menu`; both Settings pages always include it in their subtitle. These reminders do not add startup text to Original gameplay's console. The packaged controls document identifies F10 explicitly.

T-Deck shows `Alt+M: System Menu` in the corresponding places and retains Mic/back behavior, trackball navigation, and all existing shortcuts. A menu reminder at the title describes the in-game shortcut; it does not change title navigation into opening an in-game menu.

## Save and settings compatibility

Only presentation strings and the human-readable export note change. `SAVED.GAM` / `SAVED.OOL`, conversion logic, native generations, settings serialization/defaults, import/export paths, `/ultima5` SD layout, and Windows data locations are preserved. Tests retain exact file sizes, round-trip state checks, overwrite confirmations, cancellation, and persistence assertions. Technical Pc-prefixed APIs and historical resource names are intentionally retained.

## Validation and visual evidence

Results and remaining limitations are recorded below after execution. Before/after Windows images come from the actual executable using synthetic SDL input and its screenshot/readback functions, with offscreen video and scratch save folders. They are executable captures, not mockups. Both interface modes cover main menu, title/in-game Settings, DOS transfer/import/export, original View at two times, the Summoning narrative, System Menu, and gameplay. Original gameplay remains 320x200; Enhanced remains 320x240. Menu/frontend screens use the existing 320x240 composition in Original's window.

T-Deck captures are explicitly **host-rendered framebuffer captures** from the real Board/runtime over a fake ST7789. They are not photographs and do not prove physical LCD, keyboard, trackball, timing, SD, or audio behavior.

## Final review and known release limitations

A separate final audit pass checks menu reachability, historical wording, keyboard shortcuts, all text sizes, profile boundaries, save/settings compatibility, firmware source lists, packaging whitelist, and third-party attribution. Confirmed defects must be corrected and affected tests rerun before packaging.

The existing `Transfer from Ultima IV` entry still reports that transfer is deferred. This polish does not implement that feature or silently remove its historical entry. Exact DOS startup cadence and all previously qualified preservation limitations remain as documented in the historical records.

## Remaining manual acceptance

1. Windows: inspect the menu and Settings in both modes, at all text sizes and scaling/fullscreen options; confirm F10 and Alt+M pause/resume at command, direction, roster, shop, and combat contexts without consuming a turn or command.
2. Windows: verify all Summoning pages/art and let the View run through its four scenes and restart; check menu cancellation and saved settings after restart.
3. T-Deck: install the separately identified Launcher app image only when ready for user testing; cold boot, view all title/Settings rows and footers at Small/Medium/Large, play the View and Summoning, use Alt+M/Mic and trackball, change/restore settings, reboot, and confirm persistence.
4. T-Deck: import/export a copied DOS save; play a native save in world, dungeon, shop, and combat; listen for music/SFX continuity and confirm SD and launcher behavior.

Compiler stack frames and ELF section counts are static build evidence, not whole-call-chain bounds, measured free heap, or stack high-water marks. RC2 is not hardware-validated until the user reports acceptance.

## Completed engineering verification

| Gate | Result |
| --- | --- |
| Unchanged RC1 Windows baseline | 38/38 pass |
| Unchanged RC1 native baseline | 212 checks resolved PASS: initial 203 pass, nine setup failures; all nine pass after exact RC1 LF/private-dialogue/sdkconfig setup |
| Complete polished native host suite | 213/213 pass; includes 144 all-size handheld UI assertions |
| Complete polished Windows suite | 40/40 pass, including both new UI-polish scenarios |
| Relevant TypeScript preservation tests | 11/11 pass |
| Release identity/scenario-option tests | 7/7 pass |
| Differential negative controls | 4/4 pass |
| Capability boundary mutations | 8/8 killed; exact source restored; unmutated PASS |
| Release tooling mutations | 2/2 killed; exact source restored; all seven restored tests PASS |
| T-Deck compile and budget | PASS; ESP-IDF 6.1, Xtensa GCC 15.2.0, Python 3.14.5; unchanged warning/stack flags and sdkconfig |
| Physical T-Deck acceptance | NOT RUN |

Each timing mode compares 1,065 walks / 8,514 snapshots against the frozen RC1 executable and traces. Exactly eight snapshots in two walks differ: E-3 steps 5–10 (Settings) and E-5 steps 5–6 (title menu). Only framebuffer `gram` hashes change. Game state, inputs/modes/prompts, transcript, turns/clock/RNG, saves and audio match exactly; dynamic and fixed-handheld candidates agree byte-for-byte. The DOS positive control changes 1,056 walks. Final traces are checked again from committed source.

The golden proof compares eight full-screen framebuffer captures against the verified RC1 host executable. Five states are pixel-identical, including every gameplay state and the System Menu. Only the Settings subtitle/value rows and the main menu's two utility rows change (1,044 Medium Settings pixels, 1,681 Large Settings pixels, 519 title-menu pixels). Every changed pixel lies in its enumerated menu region. Three full-screen golden hashes are updated; all five others remain exact. Selection-move checks now require precisely two menu rows plus the changed category strip; value changes still require exactly one row. Assertions were retained and made explicit rather than relaxed.

Evidence: `baseline-host-tests.log`, `baseline-host-final-corrected.log`, `baseline-setup-correction.json`, `polished-host-tests.log`, `polished-windows-tests.log`, `golden-region-proof.json`, `scope-review.json`, and the differential/mutation logs under the external evidence directory. Before captures are in `before/original`, `before/enhanced` and `before/tdeck-host-rendered`; final captures are in `after/original`, `after/enhanced` and `after/tdeck-host-rendered`. The intermediate `review-after` and golden-before/after images are retained.

The initial four-job native rebuild hit a GCC 16.2.0 internal compiler error in `stl_uninitialized.h`; the unchanged build completed at two jobs. The initial firmware Ninja launch needed an explicit installed Ninja path; the subsequent two-job build passed. These failed attempts remain in separate logs. No optimization/warning flags or test assertions were weakened for those setup/compiler issues.

The uncommitted review firmware is preserved separately with its ELF and configuration. Clean-source firmware measurements and final package identities are recorded in the following closeout and the external manifest; no review image is presented as a release package.

## Clean-source firmware and release closeout

Clean production source `a264d43c18a98716aa5c3265c45013e9d5764a44` passed the firmware build and capability audit. The following documentation-only closeout commit is rebuilt for both final packages; their exact full source hash is in [the final artifact manifest](C:/Dev/U5Native/builds/ui-polish/evidence/final-release-manifest.json). The final verifier checks identical static footprint after that rebuild.

| Quantity (bytes) | RC1 | RC2 | Difference |
| --- | ---: | ---: | ---: |
| App image | 1,075,088 | 1,075,504 | +416 |
| App partition free | 235,632 | 235,216 | -416 |
| Launcher allocation free | 39,024 | 38,608 | -416 |
| .flash.text | 738,518 | 738,902 | +384 |
| .flash.rodata | 236,988 | 237,020 | +32 |
| .dram0.data | 22,091 | 22,091 | +0 |
| .dram0.bss | 54,096 | 54,096 | +0 |

App partition: 1,310,720 bytes. Launcher allocation: 1,114,112 bytes. IRAM sections remain unchanged. Existing frontend-view and System Menu compiler frames remain at 144 and 64 bytes respectively. The generic menu-body lambda frame changes from 144 to 208 bytes, with a new 32-byte settings-line helper and a new 32-byte category leaf. The largest project frame remains DeviceSmokeTests::run at 5,920 bytes; export remains 5,392 and dungeon gem view 4,928. Main/audio task stacks remain 24,576/6,144 bytes. Static RAM data/BSS are unchanged. No hardware free-heap or high-water measurement is claimed.

Final Windows ZIP: `UltimaV-Native-v0.5.0-rc2-Windows-x64.zip`.
Final raw Launcher app image: `UltimaV-Native-v0.5.0-rc2-TDeck-Launcher.bin`.
Both are placed together under `C:\Dev\U5Native\releases\v0.5.0-rc2\<full-source-hash>\ui-polish`. The [manifest](C:/Dev/U5Native/builds/ui-polish/evidence/final-release-manifest.json) records exact paths, full SHA-256 values, clean source identity, toolchains, sizes, suite totals, differential classification, package smoke results and pending hardware acceptance. The final ZIP has the established ten-file whitelist and attribution; proprietary game data stays outside it. The BIN is a validated ESP32-S3 app image, not a merged flash image.

[Windows Original captures](C:/Dev/U5Native/builds/ui-polish/evidence/after/original), [Windows Enhanced captures](C:/Dev/U5Native/builds/ui-polish/evidence/after/enhanced), and [host-rendered T-Deck captures](C:/Dev/U5Native/builds/ui-polish/evidence/after/tdeck-host-rendered) include menu, Settings, DOS transfer, intro/View, shortcut and gameplay evidence where applicable. Windows captures also include Large Settings and the long imported-save subtitle. Capture provenance records the actual executable hash and source commit. The final extracted-package smoke checks missing-data handling, bundled asset preparation from the user's game, portable launch and Original/Enhanced scenarios.

Existing RC1 ZIP SHA-256 remains `97e2ae5201076d2f96e51e80063382f7116e39c52d3cbb337faf285e2db51e54`; the RC1 Launcher image remains `742dffcfc2675d02473e1566951af7ff36d743b205051aaac40d72f6893ec787`. Final safety verification compares every other worktree HEAD/status and both original artifact hashes. RC1 and existing saves/packages are preserved.

Remaining release acceptance: physical T-Deck tests and the manual Windows checklist above. Ultima IV transfer remains unavailable, and prior documented timing/preservation qualifications still apply. No new unresolved gameplay, input, save, audio, packaging or build defect remains in the exercised engineering scope.
