# Windows — Look fidelity and Enhanced UI polish (LOOK-POLISH1)

**Branch** `windows/look-enhanced-polish` · **worktree** `C:\Dev\uvw-look-polish` · **base** `00ae0410`
(ORIG-DOS2 HEAD: its package `UltimaV-Native-Windows-x64-ORIGDOS2-caa20e72-test.zip` was built from
`caa20e72`; `00ae0410` adds only its record) · **date** 2026-10-14 · **not pushed, merged, published or
flashed.** The Windows, SPP1, SPP2, Original DOS and `main` worktrees were not written to; the
frozen SPP1 firmware and its source are untouched. A throwaway detached worktree
`C:\Dev\uvw-lp-base` (at `00ae0410`) holds the differential's baseline.

| Word | Meaning |
|---|---|
| **FIXED** | Changed in this branch, built, tested (§6); in the test ZIP (§9). |
| **CORRECT** | Investigated; already what the 1988 game does; unchanged. |
| **REMAINING** | Recorded, not changed here; the reason is given. |

## 0. Summary

1. **Look North in Britain: a real defect, FIXED (D-117).** The 1988 Look describes the adjacent
   cell, as the port does — but when that cell is a hanging sign's post or bracket (tiles 0xE0 /
   0xE1 / 0xE2), LOOKOBJ's `look_dispatch` first follows the post to the sign it holds and describes
   the sign. Neither the TypeScript reference nor native ported that loop, so they printed LOOK2.DAT's
   placeholder phrase for the post tiles, `parched desert`. The fix is in the shared core (and the
   reference), so Windows Original, Windows Enhanced and the T-Deck all now say, for the reported spot,
   `Thou dost see the sign of an armoury`.
2. **Enhanced shop prices: FIXED.** The amounts were right; the `g` suffix of the handheld's 5×7 face
   looks like a `9` (`1g` read as "19"), rows whose offer has a quantity > 1 had a narrower name field
   that pushed their price two cells left, a four-digit price shifted one cell, and a companion list
   showed `0g`. Every price now ends in one right-aligned column as `<n> gp`, with a cell of margin.
3. **Bounded Enhanced audit: two more row defects FIXED** (Ready / Use rows cut `[equipped]` and
   counts; the Ready detail line overflowed its box). Nothing else genuine found (§4).
4. **Original is pixel-identical** on every list, shop and console screen compared (§5); its only
   change is the Look text, which is the 1988 game's own.

## 1. Starting point and baselines

`git worktree list` showed `windows/original-dos-interaction` at `00ae0410` in `C:\Dev\uvw-original-dos`
(clean apart from one untracked log). The new branch and worktree were cut from it; the git-ignored
inputs (packs, map fixtures, `loot_names.inc`, `loc17_slots.inc`, `sdkconfig`, the shop / dialogue /
arena corpora) were copied and `original/`, `game/assets`, `node_modules` joined by junctions, as in
the earlier worktrees.

| Suite | Baseline at `00ae0410` | Log |
|---|---|---|
| Windows ctest (MSVC, Release) | **35 / 35** | `native/core/lookpolish-baseline-windows.log` |
| Host suite (GCC 16, `-j 4`, serial ctest) | **208 / 208** | `native/core/lookpolish-baseline-host-ctest.log` |
| `game/` vitest | 8,259 tests, **116 failing** — the pre-existing, environment-bound set SPP2 recorded | `native/core/lookpolish-vitest.log` |

## 2. Objective A — Look North in Britain

### 2.1 The report, reproduced

Windows Original, Britain, on the brick road among buildings and grass: Look North →
`Thou dost see parched desert.` The scripted scenario `look_signpost` (§6) walks the real executable
from Britain's gate (15,30) up the road to row 21 and west to **(9,21)**, then presses `l`, Up. On the
unmodified `00ae0410` executable it printed exactly `Thou dost see parched desert`, in Original and in
Enhanced (`C:\Dev\uvw-look-polish-evidence\before\look-*`). The screenshot shows why the text looks wrong:
straight north of the party stands the armoury's hanging sign (a shield on a post-and-bracket frame).

Britain's map (TOWNE.DAT map 2 = location 2, floor 0) around the spot:

| | x 8 | x 9 | x 10 |
|---|---|---|---|
| y 19 | `0xE1` bracket | **`0xF2` the sign of an armoury** | `0xE2` bracket |
| y 20 | `0xE0` post | `0xE0` post | `0xE0` post |
| y 21 | `0x44` cobble (road) | `0x44` **party** | `0x44` cobble |

### 2.2 What the 1988 Look does (the binary)

Read with `re/tools/dis16.py` from the player's `LOOKOBJ.OVL` (the notes' `re/disasm` does not exist):

- **`cmd_look` 0x099c.** `0x09a2` asks the direction (`prompt_direction`); the target is
  `x = g_party_x + dx` (`0x09ac-0x09b5`), `y = g_party_y + dy` (`0x09b8-0x09c1`) — **the adjacent cell**,
  not a distant one. `0x09c8` reads the tile through `get_tile_ptr` (raw 0xffffa172, the composited
  view: an actor covers the terrain); `0x09de` looks for an object there. `0x09e4` tile 0x29 → the
  crystal ball; otherwise `0x0a40` prints `\nThou dost see\n`; an object → its own phrase (`0x0a50`);
  the five sign faces 0x89 / 0x8A / 0xA0 / 0xA4 / 0xF8 → the sign reader (`0x0a7d`); everything else →
  **`look_dispatch(tile, x, y)`** at `0x0a95`.
- **`look_dispatch` 0x0502**, first thing:

  ```
  0510: cmp si, 0xe0 / je 0x522      ; si = tile
  0516: cmp si, 0xe1 / je 0x522
  051c: cmp si, 0xe2 / jne 0x552     ; not a post: go on to the sky, well, fountain, generic text
  0522: 0xe0 -> 0536: dec [bp+4]     ; y - 1
        0xe1 -> 054a: inc di         ; x + 1
        0xe2 -> 054e: dec di         ; x - 1
  0539: push di / push [bp+4] / call 0xffffa172   ; re-read the tile at the new cell
  0546: mov si, ax / jmp 0x510       ; and test again
  0552: mov [bp+6], di / mov [bp+8], si           ; the dispatch goes on with the chased cell
  ```

  Only then come the sky (0x59), the well (0xA1), the fountains (0xD8-0xDB), `look_generic` (the
  LOOK2.DAT phrase), the clock, the Flame and the collapsed entrance.

So the answers the brief asked for:

| Question | Answer |
|---|---|
| What does DOS Look examine? | The adjacent cell in the chosen direction — then, if that cell is a sign's post (0xE0) or bracket (0xE1 / 0xE2), the cell the post points at, repeatedly. |
| Adjacent, distant, objects? | Adjacent terrain (composited with actors); an object there takes precedence; posts resolve to their sign. |
| Different by location? | No: the same routine for the overworld and every town / castle / keep (the dungeon's Look is DNGLOOK.OVL, a separate routine, unaffected). The sky / Flame / dungeon-entrance cases depend on the tile, the clock and the location, not on the map kind. |
| Does native read the right tile? | Yes — the adjacent cell, composited, as the binary. |
| Does the text match the tile? | Yes, LOOK2.DAT's phrase for tile 0xE0 *is* `parched desert` — a placeholder the game never prints, because the chase above leaves the post before `look_generic` runs. |
| Is the visible output wrong? | **Yes.** DOS prints `Thou dost see the sign of an armoury` there. |

**The data.** LOOK2.DAT gives `parched desert` to 0x07 (real desert), 0x1C, 0x1E, 0x1F, 0xDD and the
three posts 0xE0-0xE2. Only the posts are remapped by the code. In the 1988 maps:

| Map file | 0xE0 / 0xE1 / 0xE2 cells | Every chain ends on |
|---|---|---|
| TOWNE.DAT | 71 (49 / 10 / 12) | a sign 0xF0, 0xF2, 0xF3, 0xF4, 0xF6, 0xF7 or 0xF9 |
| CASTLE.DAT | 45 | a sign 0xF0, 0xF2-0xF7 or 0xF9 |
| KEEP.DAT | 6 | 0xF4 (stable) |
| DWELLING.DAT, BRIT.DAT, UNDER.DAT | 0 | — |

122 posts, no loop, no chain leaving its map, none ending on the sign face 0xF8. 0x1C / 0x1E / 0x1F
do occur on the overworld (and in dwellings) and print `parched desert` in DOS too — **CORRECT**,
unchanged.

**DOSBox-X.** The adjudication is static: the routine is short and unambiguous, and the reproducer
runs the real map data. A DOSBox-X capture of the same Look in the 1988 game was **not** taken (it
needs the 1988 party walked to Britain's armoury); it is listed as an optional manual check (§10).

### 2.3 Root cause

The RE ledger already had it: `re/ledger/frontier-manual.json` verifies `look_dispatch` 0x0502
body-to-body ("BUCLE DE ANCLA de objeto multi-casilla…") and `re/notes/lookobj.md` lists the post
canonicalisation. The port never implemented it: when the clone's sign set was corrected (that note's
divergence 1, "sobraban 0xE0-0xE3 postes"), the posts were removed from the sign branch but the chase
was not added, so they fell through to `look_generic`. The TypeScript reference (`Game.look()`) and
native (`world_look`, `native/core/src/look.cpp`) shared the gap. **A shared gameplay defect**, new
ledger row **D-117** (`docs/hardware/ALPHA2_PRESERVATION_LEDGER.md`; `re/notes/lookobj.md` item 3).
No earlier ledger item covered it.

### 2.4 The fix (FIXED)

At the layer the parity fixture pins (`gameplay_parity` drives `Game.look()` and `world_look` side by
side): both now run the binary's loop after the sign-face branch and before the special cases —
0xE0 y−1, 0xE1 x+1, 0xE2 x−1, the coordinates wrapped on the overworld, the tile re-read *composited*
(an actor on the chased cell is what is seen, as with the first read). The chased x is what the rest
of the dispatch sees, as `[bp+6]` is in the binary. Two port-side guards the binary lacks and no map
needs: the loop stops after 64 steps (a loop would otherwise hang the device), and a chase that leaves
a town's map reports `darkness` like any other off-map Look.

- `native/core/src/look.cpp` — `world_look`: the composited read is one lambda used by both reads; the
  chase.
- `game/src/core/game.ts` — `Game.look()`: the same, in the reference.

`gameplay_parity` is a live comparison (no stored corpus), so nothing was regenerated. With only the
native half applied it went RED at sequence 1810 (`edits [[10,9,224]]`, Look north: native
`Thou dost see cobble`, reference `Thou dost see parched desert`); with both halves it passes
(`native/core/lookpolish-gameplay-parity-*.log`). Enhanced and Original print the same core message.

## 3. Objective B — Enhanced shop prices (FIXED)

### 3.1 What the screen showed

The handheld's shop panel (Windows Enhanced and the T-Deck draw the same `tdeck::Board`) wrote each
offer as `" %-14.14s %3ldg"`, or `" %-12.12s %3ldg"` when the offer's quantity was above 1
(`tdeck_board.cpp`, unchanged since `c18f5b64`, Alpha 2). With Iolo's Bows (Britain's blacksmith;
Gwenneth), the user's `19 / 139 / 1009 / 139 / 2019 / 209` were **amounts followed by `g`**:

| Cause | Effect |
|---|---|
| The 5×7 face's `g` ({0x0c,0x52,0x52,0x52,0x3e}) is the `9` ({0x06,0x49,0x49,0x29,0x1e}) one row lower | `1g` reads "19", `13g` "139", `100g` "1009" |
| Arrows and Quarrels are offers of quantity 99 (reagents and the guild's items carry their grant) | their name field was 2 cells narrower, so their price sat 2 cells (12 px) left of the others — and the count was never shown |
| `%3ld` | a 4-digit price shifts one cell right of the 3-digit ones |
| A companion list (healer, inn) is offers of price 0 | `Shamino 0g` |

Measured on the real Board (rightmost ink column of each row, `look_polish_runtime` S1 on the old
code): **302 302 302 290 308 290** for Dagger 1 / Sling 13 / Bow 109 / Arrows 13 (×99) / Crossbow 1009
/ Quarrels 20 (×99). The amounts themselves were right; no price, purchase, inventory, gold or RNG
code changed (the same Iolo's Bows list before and after, deterministic host run:
`Dagger=1 Sling=14 Bow=105 Arrows=14 Crossbow=210 Quarrels=21` at INT 20).

### 3.2 The new row

`tdeck::format_shop_row` (`device_ui_views.h`), used by the Board for every offer row, 22 cells of 6 px
in the panel's 134 px box:

- cell 0 the gutter; the name from cell 1, left-aligned;
- the price **right-aligned to end at cell 20**, written `<n> gp` — the space and the two-letter unit
  keep the suffix from reading as a digit; cell 21 is a blank margin (9 px to the panel's right rule);
- at least one blank cell between name and price; a name that does not fit is cut and ends in `.`
  (`Ring of Reg. 1500 gp`), never into the price;
- a **Sell** row shows the party's count (`Dagger x6`), kept whole when the name is cut; Buy shows no
  count (an offer's quantity there is a stack or grant size, not something the player holds);
- a **companion** row (the healer's and the inn's lists) has no price at all.

The runtime marks the last two (`DeviceShopRow::counted` / `priced`, `compose_shop_view`); the
Board's row cache compares them too. After: **308 308 308 308 308 308**, the margin 9 px, the
selected row still the reverse-video bar, on every page (Buy, Sell, reagents, guild) and every
selection.

| Price | Row |
|---|---|
| 1 | ` Dagger          1 gp ` |
| 13 (×99) | ` Arrows         14 gp ` |
| 109 | ` Bow           109 gp ` |
| 1009 | ` Bow          1009 gp ` |
| 20190 | ` Bow         20190 gp ` |
| Sell, 6 owned | ` Dagger x6       1 gp ` |
| companion | ` Shamino              ` |

**Who sees it.** The handheld screen is shared, and the old rows were equally ambiguous on the T-Deck
(the same face, the same 134 px box); the new row fits the same cells. So the change is universal: the
T-Deck shows the same rows (§7). Windows Original has no shop panel (console shops) and is unchanged.

## 4. Bounded Enhanced audit

Captured from the real executable (`C:\Dev\uvw-look-polish-evidence\after\audit`, contact sheet
`sheets\enhanced-audit-contact.png`) and measured against the name tables (`display_names.cpp`).

| Panel | Finding | Verdict |
|---|---|---|
| Ready list | The selector shows 21 cells after its gutter; `format_ready_row`'s `Leather Armour [equipped]` (25) became `Leather Armour [equipp`, `Chain Coif x1 [equipped]` lost its marker | **FIXED**: `fit_list_row` keeps a row that fits as it is; otherwise writes the marker `[eq]`, and only then cuts the name with `.` — `Leather Armour [eq]`, `Chain Coif x1 [eq]`, `Ring of Invi. x2 [eq]` |
| Ready detail | `Current: %.18s` in a 22-cell row: `Current: Leather Armour` ran past the box | **FIXED**: the name gets the 13 cells after `Current: ` (`Current: Leather Armo.`) |
| Use list | `Kal Xen Corp Scroll x2` (22) lost its count; `Sceptre of Lord British` (23) was cut mid-word with no mark | **FIXED** by the same `fit_list_row` (`Kal Xen Corp Scro. x2`, `Sceptre of Lord Brit.`) |
| Shops and services | §3 | **FIXED** |
| Cast / spell list, Mix, party / companion pickers, Z-stats | rows fit (spell names ≤ 15 + count; reagents ≤ 11); Z-stats' `Magic:19` / `Magic: 0` is ZSTATS' own 2-digit field | **CORRECT** |
| Save / Load slot screens, System Menu | the frontend's 320-wide lists; no clipping | **CORRECT** |
| List rows' last cell | a 21-character row's last glyph ends 3 px from the right rule (pre-existing; `Chain Coif [equipped]`) | **REMAINING**: not overlapping; widening the margin would abbreviate common rows |
| The 5×7 `g` | resembles `9` everywhere the face is used (names too: `Ginseng`) | **REMAINING**: a global font change (every handheld screen and pixel golden); the prices no longer depend on it |

Keys, shortcuts, mode switching and saves are untouched; the labels behind the lists (what the
console echoes and what the DOS panel composes) are unchanged — the fit is applied only where the
handheld selector draws them, and not in the 1988 model (`platform_dos_interface()`).

## 5. Original preserved

- **Code.** Original's shops are console shops (`compose_shop_view` returns before the panel), its
  lists are the DOS panel composed from the unchanged labels, and the two runtime fits are skipped
  under `platform_dos_interface()`.
- **Pixels.** `original_audit.txt` (world, `Select:`, Ready, Ready scrolled, Use, Z-stats ×2, the
  blacksmith's greeting and `Arms` window, the reagent seller), each run twice on the `00ae0410`
  executable and twice on this branch's, on the 320 × 200 DOS screen:

  | Screen | base vs base | base vs new (panel + console, x ≥ 185) | base vs new (whole) |
  |---|---|---|---|
  | world, Select:, Ready, Ready↓, Use, Z-stats 2 | 0 – 32 | **0** | 0 |
  | Z-stats 1 | 26 | **0** | 0 |
  | blacksmith greeting | 2,739 (panel) | **0** | 6 (map animation) |
  | blacksmith Arms, reagents | 2,273 / 3,157 (panel) | **0** | 0 |

  Base-vs-base differences are the scripts' wall-clock waits (greetings and clock); the paired base
  and new runs fell on the same ticks and match pixel for pixel. Every Original scenario of the
  Windows suite passes unchanged (`original_dos_panels`, `original_interface`, `original_keys`,
  `presentation_dos`, `spp1_original*`, `spp2_original`, …).
- **The one Original change** is D-117's text, `look_signpost_original` (before/after:
  `sheets\original-look-north-before-after.png`).

## 6. Tests

| Suite | Result | Log |
|---|---|---|
| Host suite (GCC, serial) | **210 / 210** (208 + `look_post`, `look_polish_runtime`) | `native/core/lookpolish-final-host-ctest.log` |
| Windows ctest (MSVC) | **38 / 38** (35 + `scenario_look_signpost_original`, `_enhanced`, `scenario_enhanced_shop_prices`) | `native/core/lookpolish-windows-ctest.log` |
| vitest | 8,265 tests; FAIL set **identical** to the baseline's 116 | `native/core/lookpolish-vitest.log` |
| Mutants | **19 / 19 killed** (§6.2) | `native/core/lookpolish-mutants.log` |

### 6.1 New tests, RED first

- `look_post` (`native/core/tests/look_post_test.cpp`, 269 checks): the 1988 TOWNE.DAT / LOOK2.DAT
  read from `original/u5/ultima5`; L1 the report (9,21) and its neighbours; L2 the four directions onto
  each post kind and three controls; L3 **every one of the 122 posts** of the four small-map files;
  L4 the overworld wrap (north and east), an actor on the wrapped cell, a post pointing off a town's
  map, a loop; L5 the composited re-read. **RED** on the old `look.cpp`: 135 failures, every one
  `parched desert` (`lookpolish-look-red.log`); GREEN 269 / 269.
- `look_polish_runtime` (`native/targets/tdeck/host_tests/look_polish_runtime_test.cpp`, 35 checks,
  real AlphaRuntime + real Board over the fake ST7789): L1-L6 Look through raw T-Deck input on the
  runtime with the pack's LOOK2.DAT; F the formatters (1-5 digit prices, the quantity rows, cut names,
  counts, companions, list rows); S the panel's pixels (one ink column, margin ≥ 6 px, the bar, no
  price ink on a companion row, the row cache); B Iolo's Bows Buy and Sell and a healer's companions
  through the real shop; R the Ready list and its detail line. **RED** on the old Board / runtime /
  `look.cpp` (`lookpolish-runtime-red.log`: ragged columns `302 302 302 290 308 290`, `[equippe`,
  `parched desert`); GREEN 35 / 35. The fixture gained an opt-in `look_text_from_pack` (its Look
  otherwise answers `something` for every tile; every other test keeps that).
- `game/tests/look-signpost.test.ts` (6): the reference on the real assets, the 122 posts included.
  **RED** on the old `game.ts`: 4 / 6 fail, the two controls pass (`lookpolish-ts-red.log`).
- Windows scenarios on the real executable: `look_signpost` in both modes (walks to (9,21), the
  armoury's sign, no `parched desert`, the road still `cobble`) — RED on the `00ae0410` exe;
  `enhanced_shop_prices` (Buy, Buy scrolled, Sell, reagents, guild: `expect shop_price_column aligned`
  read from the **presented pixels**, `x=308 margin=9`; the healer's list `no prices`; `shop_row`
  through the Board's own formatter).

### 6.2 Mutants

`native/core/tools/lookpolish_mutation_check.py`: one edit each, built and run against `look_post` and
`look_polish_runtime` (host) or the vitest file.

| # | Mutant | Killed by |
|---|---|---|
| M1 | no chase | look_post L1-L3 |
| M2 | 0xE0 points down | look_post L1 |
| M3 | 0xE1 / 0xE2 swapped | look_post L1 |
| M4 | the chase reads the bare map | look_post L5 |
| M5 | no wrap in the chase | look_post L4 (the actor on the wrapped cell; added after M5 survived the first round — `tile_at` wraps by itself, only the composited read sees the difference) |
| M6 | off-map after the chase is not darkness | look_post L4 |
| M7 | no margin cell | runtime F1, S1 |
| M8 | the old `g` against the digits | runtime F1 |
| M9 | companions priced | runtime B5 |
| M10 | Sell shows no count | runtime B4 |
| M11 | list rows cut at 22 again | runtime R1 |
| M12 | no short marker | runtime F6 |
| M13 | the row cache ignores `priced` | runtime S4 |
| M14 | the Board's old two formats | runtime S1, B2 |
| M15 | a cut name gets no `.` | runtime F3 |
| M16 | the Ready detail overflows | runtime R2 |
| T1-T3 | the reference: no chase, 0xE0 down, 0xE1 / 0xE2 swapped | look-signpost.test.ts |

## 7. T-Deck

**Intentional changes on the device** (shared code): D-117's text, and the handheld shop and list
rows of §3-§4. Nothing of Windows' DOS screen or its scenario code reaches the firmware.

**Differential** (`native/core/tools/lookpolish_differential`: SPP2's probe with every Look printing
LOOK2.DAT's phrases and a walk J for this batch; baseline `00ae0410` in `C:\Dev\uvw-lp-base`, the same
GCC and packs): **1,065 walks / 8,514 raw inputs; 1,001 identical; 64 differ, every one classified,
0 unexplained**, unpaced and paced alike:

| Class | Walks | What differs |
|---|---|---|
| P (D-117) | 5 — the J Looks at Britain's posts | the transcript (`parched desert` → `the sign of …`) and the panel; same mode, prompt, state, turns, clock, seed, audio, save at every step |
| S (shop rows) | 51 — 46 Talk-opened shop walks + 5 J shops | the panel pixels only, on shop screens |
| L (list rows) | 8 — `r` walks with long equipped names, J Ready / Use | the panel pixels only, on list screens |

The control Look at the road (15,21) is identical; the Windows Original profile (`--dos`, the positive
control) differs in 1,054 walks. Log: `native/core/lookpolish-tdeck-differential.log`.

### 7.1 Firmware

ESP-IDF 6.1, the same `sdkconfig`, built from `8570d1d3` in `native/targets/tdeck/build-look`
(`idf.py reconfigure` + `ninja -j 4`; the image names `8570d1d37e9f-dirty` — the only uncommitted
change then was this record's ledger edit). **No project warning** under the firmware's `-Werror`
flags (`lookpolish-fw-build.log`). Compared with ORIG-DOS2's build of the same base code
(`C:\Dev\uvw-original-dos\native\targets\tdeck\build-dos`, `caa20e72` = `00ae0410`'s code;
read with `python -m esp_idf_size --diff`, nothing rebuilt there):

| | Base (`caa20e72`) | This branch | Δ |
|---|---|---|---|
| App image | 1,095,328 B | **1,096,432 B** (`0x10baf0`) | **+1,104 B** |
| `.text` / `.rodata` | 757,258 / 238,508 | 758,330 / 238,524 | +1,072 / +16 |
| DIRAM `.bss` / `.data` | 54,080 / 22,075 | 54,096 / 22,091 | +16 / +16 (consistent with the shop rows' two new flags in the view copies; not attributed symbol by symbol) |
| Free in the 1.25 MiB app partition | 215,392 B | **214,288 B (16.3 %)** | −1,104 B |
| Launcher allocation | 1,088 KiB | 1,088 KiB (17,681 B to the next 64 KiB step) | unchanged |

Stack (`.su`, 2,275 → 2,280 functions, 7 lines differ): `world_look` 336 → 352 B and its new
`seen` lambda 32 B; `compose_shop_view` 64 → 80 B; the new `fit_label` 48 B, `format_shop_row` 64 B,
`fit_list_row` 112 B (called from the shop panel and the selector, not from any deep path).
`compose_selection_view` and `Board::show_alpha` are unchanged. Image SHA-256
`5c714a8d…bc294e05`. **Not flashed; no hardware validation is claimed.** Logs:
`native/targets/tdeck/lookpolish-fw-{configure,build,size-diff}.log`.

## 8. Screenshots (actual executable)

Kept out of the repository (game art), in `C:\Dev\uvw-look-polish-evidence\`:

- `sheets\original-look-north-before-after.png` — Original, Britain (9,21), Look North: `parched
  desert` → `the sign of an armoury`. Window 3× and fullscreen: `after\windows-orig\`.
- `sheets\shop-iolo-buy-before-after.png`, `shop-iolo-sell-…`, `shop-reagents-…`, `shop-guild-…`,
  `shop-healer-member-…` — Enhanced, before (`00ae0410`) and after. (Prices vary between the two
  runs: the scripted runs' wall-clock waits move the clock and the haggle; the deterministic host
  run shows the same prices before and after, §3.1.)
- `after\host\*.png` — the real Board from `look_polish_runtime` (`shop-panel-prices` with 1-, 2-, 3-
  and 4-digit prices and the quantity rows; `shop-iolo-buy`, `shop-iolo-sell`, `ready-list`).
- `after\windows-enh\` — the Enhanced shop in Window 3× (1008 × 768), at 1920 × 1080 and fullscreen.
- `after\audit\`, `sheets\enhanced-audit-contact.png` — the Enhanced audit; `scratch\orig-*` — the
  Original comparison runs.

## 9. Windows test package

- **File**: `native/targets/windows/dist/UltimaV-Native-Windows-x64-LOOK-POLISH1-5b7f8bbc-test.zip`
  (this worktree), **2,329,565 B**, SHA-256
  `270db6a794d304c51114635262ec6c1680e1b95801f941ca266a61705b65dfe7`. A new name; the ORIG-DOS1 /
  ORIG-DOS2 ZIPs (in `C:\Dev\uvw-original-dos`) and every earlier package are untouched.
- **Built** by `tools/build-windows.ps1 -Config Release -Test -Package -PackageName
  UltimaV-Native-Windows-x64-LOOK-POLISH1-5b7f8bbc-test` in a fresh `build-pkg` from `5b7f8bbc` with a
  clean tree. BUILD-INFO: `Source: 5b7f8bbc8713f5aa9ca8001fc0301d5b928ec198
  (windows/look-enhanced-polish)`, MSVC 14.51, static C/C++ runtime (no VC++ redistributable needed),
  SDL 3.4.16 (`SDL3.dll` included), `UltimaVNative.exe` SHA-256 `7d2886de…20fa94`; the asset-preparation
  bundle rebuilt from the same commit; **no Ultima V game data**. The packaging run passed **38 / 38**
  (`native/core/lookpolish-win-package.log`).
- **Smoke** (fresh scratch folder `C:\Dev\uvw-look-polish-evidence\smoke`, a fresh data folder per
  run, the player's `%APPDATA%` untouched): the packaged exe (it logs `git 5b7f8bbc8713`) passed
  `look_signpost` in Original and Enhanced, `enhanced_shop_prices` and `original_dos_panels`.
- **Not a release.** No person has played it.

## 10. Manual validation

1. Unzip the test ZIP into a new folder; copy the three `openu5-*.bin` packs (or run
   `Prepare-Game-Data.cmd`).
2. **Look.** Original: start a journey, enter Britain, walk up the road to the row of the armoury's sign
   (just below the shield-on-a-frame, west of the main road), Look North → `Thou dost see the sign of
   an armoury`. One step east, Look North → the same. On the main road, Look North → `cobble`. The
   provisioner's and inn's signs near the north gate likewise. Repeat in Enhanced (F10 → Settings →
   Interface).
3. **Shop.** Enhanced: Iolo's Bows (Talk to Gwenneth) → B: every price in one column as `<n> gp`,
   Arrows / Quarrels aligned with the rest. S: the pack's counts (`Dagger x6`). A healer → H: the
   companions without `0g`.
4. **Lists.** Enhanced: Ready with long equipment worn (`Leather Armour [eq]`); Use with several
   scrolls (counts visible).
5. **Original unchanged.** The DOS shop transcripts, the `Arms` window and the Ready / Use panels look
   as in ORIG-DOS2.
6. Optional: the same Look in the 1988 game under DOSBox-X (Britain, under the armoury's sign).

## 11. Remaining

- The 5×7 face's `g` still resembles `9` in item names (a global font change; §4).
- A full 21-character list row ends 3 px from the panel's right rule (pre-existing; §4).
- No DOSBox-X capture of the 1988 Look was taken (§2.2); the evidence is the binary and the map data.
- Hardware: the T-Deck rows and Look text are host-verified only; nothing was flashed.
- Port-side guards the binary lacks (the 64-step bound, `darkness` off a town's map) are unreachable
  with the 1988 maps.

## 12. Commits

On `windows/look-enhanced-polish`, after `00ae0410`:

| Commit | What |
|---|---|
| `8a480c9f` | D-117: the post chase in `look.cpp` and `game.ts`; `look_post`, `look-signpost.test.ts`, `scenario_look_signpost_*`; `re/notes/lookobj.md` |
| `8570d1d3` | the handheld shop and list rows (`device_ui_views.h`, `tdeck_board.cpp`, `alpha_runtime.cpp`); `look_polish_runtime`; `scenario_enhanced_shop_prices` and its two pixel checks; the fixture's opt-in LOOK2 text |
| `5b7f8bbc` | this record, the ledger row, the differential probe and classifier, the mutation driver, the logs (the package is built from it) |
| next | this record's §9: the test package |

The suites in §6 ran on the combined working tree before the split into the first two commits;
the packaging run (38 / 38) ran on `5b7f8bbc`.
