# SPP1 hardware test — handoff (prepared 2026-10-14, during SPP2)

**Test the frozen SPP1 image, not SPP2.** Nothing here has run on a T-Deck. Nothing was flashed.
Compilation is not hardware validation. The full SPP1 record is
`docs/SHARED_PRESERVATION_PARITY_SPP1.md` (§5 lists what a player sees change, §8 the original
checklist); this page is the 20–30 minute version of §8, with identity, install and rollback.

## 1. The image under test

| Item | Value |
|---|---|
| Branch / HEAD | `parity/shared-preservation-1` / `fc4a63aa` (worktree `C:\Dev\ultima-v-native-parity`, clean, unchanged) |
| Code commit in the image | `5ba2fa46` — every later SPP1 commit changes only docs and logs (`git diff --stat 5ba2fa46 fc4a63aa -- native/targets/tdeck/main native/core/src native/core/include` is empty) |
| App image (as built) | `C:\Dev\ultima-v-native-parity\native\targets\tdeck\build-spp1\openu5_tdeck.bin` (not touched) |
| **File to install** | `C:\Dev\spp1-hw-package\OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin` |
| Size | 1,072,912 B (`0x105f10`) |
| SHA-256 | `1aeecb70dec68141122e4aa88a9ce140cad87a617ff5668ec3eb8fa1d8b123c2` (the app image, the packager's output and the renamed copy are byte-identical) |
| Embedded identity | `FW 4.0.0-alpha4-rc5-debug` (the version string was never changed), `Git 5ba2fa46b74f` (found once in the image, no `-dirty`); the RC5 hash `59c14b907399` is absent |
| App partition | 1.25 MiB (`0x140000`): 237,808 B (18.1 %) free; `check_app_budget.py` passes |
| Launcher allocation | 1,114,112 B (1088 KiB; the image rounded up to 64 KiB); the next 64 KiB step is 41,201 B away |
| SD packs | **unchanged from Alpha 4**: `/ultima5/openu5-alpha1-resources.bin` 2,266,819 B, CRC32 `5c0d175d` (SHA-256 `85b38994…d01e`); `/ultima5/openu5-assets.bin` (SHA-256 `6eb001ed…e188`); `/ultima5/openu5-audio.bin` as before. SPP1 adds no save field: saves load both ways. |

**How the package was made** (`C:\Dev\spp1-hw-package\spp1-package.log`). The project's own packager,
`package_launcher.py` with `check_app_budget.py`, both taken from `fc4a63aa` with `git show`, ran with
`python -B` against a staging copy (`build-spp1-staged\`: the image, `project_description.json` and
`partition_table\partition-table.bin`, copied from `build-spp1`). Nothing was written into the
SPP1 worktree. The packager validated the header, the segment checksum and the appended SHA-256, and wrote
`build-spp1-staged\launcher\OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin`.

**Filename trap.** That packager name is the Alpha 4 RC5 release's name, because the version string
is RC5's. The renamed copy `OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin` has the same bytes. Put only
the renamed file on the card, and trust the identity screen's `Git` line, never the file name.

## 2. Before installing

1. **Write down what is installed now.** The boot identity screen shows `FW …` and `Git …`. This
   is the rollback target (§6).
2. **Back up the card's saves:** copy `/ultima5/saves/` and `/ultima5/settings.json` to the PC.
   SPP1 doesn't change the save format, but SPP1 turns cost more time than Alpha 4's, so a save made
   during the test has a later clock.
3. The image is over the 1 MiB `ota_3` slot of the Launcher layout. **Install it through the
   Launcher, not with `esptool`** (`native/targets/tdeck/LAUNCHER.md`; `docs/history/alpha/ALPHA4.md`, the
   install line; `A4-FINAL-HW.md` §1 in the main checkout).

## 3. Install

Copy `OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin` to the SD card and install it from Launcher (**SD** →
the file). This is the documented Alpha 4 procedure. Then boot.

**Gate (PASS/FAIL):** the identity screen reads `Git 5ba2fa46b74f` and the packs match (`RES v… 2266819B
CRC 5c0d175d`). `Git 59c14b907399` is the old RC5, and `Git 96a55ff9084f` is main's A4-FINAL-HW build.
Either means the wrong image is installed: stop and reinstall.

## 4. The checklist (~25 min, in priority order)

Set the state with the Developer tools (Alt+D in game): Shortcuts/Presets → *Max Resources* (gold,
spells, reagents), *Equip Best Gear*; Inventory → *Scroll quantity* / *Potion quantity*; Stats →
*Current MP*; Teleport for towns and dungeons; Time → *Hour*. The clock is
on the status panel. Record each row as **PASS / FAIL / NOT RUN**, with a note.

| # | Area (SPP1 item) | Steps | Expected on the SPP1 image |
|---|---|---|---|
| 1 | Identity | §3 gate | `Git 5ba2fa46b74f`, packs match |
| 2 | Smoke | Title → load your save (or New); walk; open and close the System Menu (Alt+M) | Normal play, no hang. Alt+S / Alt+L work. |
| 3 | Turn cost (A), overworld | Note the minute. R + Mic, Z + Mic, N + Mic, U + Mic, C + Mic, L + Mic, each separately | Each adds **+2 min** (one world turn), even though it was cancelled |
| 4 | Turn cost (A), town | In a town: R + Mic, then L + Mic | **+1 min** each |
| 5 | Conversation (A) | Talk to a townsperson, end with an empty answer (Enter) | The clock does **not** move |
| 6 | Mix timing (A, `5ba2fa46`) | M, a spell you have reagents for, mark them, M, `1`, Enter: `Mixing...`, then `Done!`. Wait 5 s, watch the clock. Then Enter (Pass). | **Known SPP1 behaviour:** the clock does not move after `Done!` until the next key; after the Pass it is **+4 min** in all. A double charge (+6) or a lost turn (+2) is a FAIL. (SPP2 moves the Mix's turn to the moment `Done!` appears, as DOS does.) |
| 7 | Map reveal (A) | U a View scroll (Inventory → Scroll quantity) on the surface; press letters during the reveal | The letters do nothing: no turn, no command |
| 8 | Arena (C) | In a fight, with no keys: J → `No Keys!`. P at bare floor → `Won't budge!`. Y, `ORT` → `No effect!`. J, then Mic at `Jimmy-` | Each of J / P / Y ends the turn; Mic at `Jimmy-` is free |
| 9 | Dungeon (E) | Teleport into a dungeon. L → pick → Up: `You see: …`. D: the Look aimed Here. On a fountain, D → `Will you drink?` → Y | `Look...`, `Player:`, `Dir-`; the picked member drinks; each costs one dungeon turn; there is no `No fountain here.` |
| 10 | Cast / Use (F) | Mani, then Mic at `On who:`. A potion, then Mic at `Use on whom?`. Mani with 0 MP (Stats → Current MP) | The charge and MP are spent; the potion is spent (`Potion` echoed); with 0 MP, `M.P. too low!` and no picker |
| 11 | Arena spells (D) | In a fight: An Xen Corp / In Por / Wis Quas | They cast with no reticle |
| 12 | Tavern (B) | Moonglow, Sam's tavern: buy a drink, then T, `brit`, Y | A gossip line (its form varies) and `says Sam.` |
| 13 | Save round trip | Alt+S, Alt+L, then open the System Menu → Load | `Save complete` / `Load complete`; the game continues |

Optional, if time allows: SPP1 §8 items 3 and 5 in full: darkness in a dungeon (`darkness.`), and
An Ylem at a field in a town (`POOF!`).

## 5. If something fails

- **Photograph the screen**, including the transcript lines above the prompt.
- Note the identity line, the step, the key presses and the clock before and after.
- If a USB serial monitor is attached, keep the log. The useful lines are `IDENTITY`, `INPUT_ROUTE`,
  `DIALOGUE_PAUSE`, `MAP_REVEAL_INPUT` and `UI_MODE`.
- The SD diagnostic log is off at boot. Developer → Diagnostics → *Probe: SD diag logging* turns it on
  for the session (A3-04D). Turn it on only to reproduce a failure, then copy the log from the card.
- Keep the save the failure happened on (copy `/ultima5/saves/`).

## 6. Rollback

Reinstall what was there before (§2.1), through the Launcher, the same way:

| Was installed | File | SHA-256 | Identity |
|---|---|---|---|
| Alpha 4 release (RC5) | `C:\Dev\OpenU5-Native\native\targets\tdeck\build-a4-rc5\launcher\OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` (1,047,408 B) | `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210` | `Git 59c14b907399` |
| main `96a55ff9` (A4-FINAL-HW build, if that was on the device) | `C:\Dev\OpenU5-Native\native\targets\tdeck\build-core\launcher\OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` (1,049,504 B) | `9388b598a59030f46a3cd0575ac91baad76d50bb88b07cbed94e0b8f361efdd7` | `Git 96a55ff9084f` |

All three images use the same file name. **Check the SHA-256 before you copy, and the `Git` line after
you boot.** If needed, restore the save backup from §2.2. The packs are the same for all three.

## 7. Record

Write the results under a dated heading in a new file, e.g. `native/targets/tdeck/SPP1-HW.md`. Don't
edit the SPP1 record. Then decide whether SPP1 (and later SPP2) may be merged or flashed.
