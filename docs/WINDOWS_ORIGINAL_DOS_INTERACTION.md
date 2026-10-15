# Windows Original — the 1988 game's own selection screens (ORIG-DOS1)

**Branch** `windows/original-dos-interaction` · **worktree** `C:\Dev\uvw-original-dos` · **base**
`bf3480e3` (SPP2 HEAD, unchanged) · **date** 2026-10-14 · **not pushed, merged, published or flashed.**
The frozen SPP1 / SPP2 branches, `main` and the Windows worktree (`windows/initial-port`) were not
written to.

The first manual Windows playtest of Original mode found that pressing **U** opened a highlighted
`Items:` list, **R** an equipment list with `[equipped]` labels, and that DOS-style `Item:` /
`Player:` prompts were driving the handheld's list panels; the desktop picture looked mostly black,
with no obvious frame. This batch checks what was running, establishes how the DOS game shows each
choice, corrects Windows Original to match, and leaves Enhanced and the T-Deck as they were.

| Word | Meaning |
|---|---|
| **IMPLEMENTED** | In this branch, built, tested (§5); in the test ZIP (§8). |
| **MOCKUP** | A picture made from real captures to show an option; nothing in the code. |
| **REMAINING** | Recorded, not changed here; the reason is given. |

## 1. What was running

| Check | Result |
|---|---|
| Package played | `UltimaV-Native-Windows-x64-SPP2-test.zip`, 2,279,964 B, SHA-256 `c4384afd…983f` (the SPP2 record, §7.4) |
| Embedded identity | `UltimaVNative.exe` holds `0bfa8e3a9d2b`; its log line reads `Ultima V Native for Windows 0.5.0-dev (git 0bfa8e3a9d2b, …, built Oct 14 2026 17:44:42)`; BUILD-INFO `Source: 0bfa8e3a… (parity/shared-preservation-2)` |
| SPP2 HEAD | `bf3480e3`; `0bfa8e3a..bf3480e3` touches only the SPP2 record and two logs, so **the package is current, not stale** |
| The playtest's own log (`%APPDATA%\Ultima V Native\ultima5\logs\windows.log`) | both sessions of the build above: `WINDOW 3x asked, 3x bare client 960x720` and `ready: interface=Original window=3x (saved) scaling=Integer` |
| Where the reported behaviour lived | in the SPP2 source: `compose_selection_view()` kept the handheld's selector panel in Original and only swapped its title (`Items:`, the member's name); `format_ready_row()` wrote `[equipped]`; `compose_shop_view()` kept the shop panel. The fidelity matrix called these rows "DOS" because DOS **does** list Use/Ready/Mix/Z-stats in its right panel -- but what was delivered was the handheld's panel, not DOS's |

Original's feature set at SPP2 (unchanged here, `ORIGINAL_MODE_FIDELITY.md`): the roster picker for
member choice, typed spell names, the 1988 prompts and key rules (getdir, Y/N, text entry, `What?`),
(Q)uit & Save, the arena's turn endings, the SPP1/SPP2 shared parity rules, Ctrl-K/V/S.

## 2. How the DOS game shows its choices — evidence

- **Static** (binary offsets, already in the repository): `re/notes/use-picker-panel.md`,
  `re/notes/ready-picker-panel.md`, `re/notes/ztats-layout.md`, `native/core/spp1-findings/B-shops.md`
  and the web port's faithful skin (`game/src/skin/fiel/*`, `game/src/ui/shop-console.ts`).
- **Live**, this batch: the DOSBox-X 2024.03.01 that ships with the player's own install
  (`original/u5/ultima5`), run on a scratch copy with the install's own `SAVED.GAM`, keys typed with
  DOSBox-X's `AUTOTYPE`, frames read back with `PrintWindow` (320 × 200). Captured: the world, U, R
  (the `Select:` roster then the list), Z (stats and Arms pages), M (the reagent list, a marked
  reagent). Shops were **not** captured live (that needs a save standing at a merchant); for them the
  evidence is static. The captures are game art and stay out of the repository (CONTRIBUTING); they
  are in `C:\Dev\uvw-original-dos-evidence\dos-1988\`, with the scripts in `…\tools\`.

**Measured DOS geometry** (pixels of the 320 × 200 screen; text cells are 8 × 8):

| Row(s) | What |
|---|---|
| y 0..6 | the frame's blue band; the panel banner `►Name◄` (kernel 0x4e50) centred in a black notch with the band brackets |
| y 7 | the box's top white rule (broken under the notch) |
| y 8..79 | text window 1 = cols 24..39 × rows 1..9; a list's scroll frame is cols 0..14 × rows 0..8, content rows 1..7 at y 16..64, content cells cols 1..13 |
| y 80 | the box's bottom rule; y 81..86 the band with the arrow notch (`↓` 0x19 / `↑` 0x18 / `↕` 0x12 at col 31, kernel 0x4dea) |
| y 87 | the console's top rule; the console is rows 11..24 (y 88..199), **left whole** while any list is open |

## 3. Audit — each interaction

| Interaction | DOS (display / input) | SPP2 Original (playtested) | Now (Original) |
|---|---|---|---|
| **(U)se** | `Use item`, `Item: ` in the console; `►Items:◄` panel over the roster/status boxes; 7-row scroll frame; rows `NN` + space + the 0x1916 entry: scrolls `0x1c + ` + initials in RUNES.CH, potions `0x1d + ` + colour, `Magic Crpt`, `Shard/Hatred`…; no count for the 0xff items; id order (scrolls, potions, carpet, keys…); bar cols 1..13; ↑/↓ clamp, PgUp/PgDn 7, RETURN/Space use, ESC `None!` | the handheld's right-column panel (14-px rows, `Yellow Potion x3`, potions first), console cut to a strip | **IMPLEMENTED**: the DOS panel as described, names from the player's DATA.OVL (pack), id order; keys unchanged (already DOS) |
| **(R)eady** | `Ready...`, `Player: ` on the roster (`►Select:◄`), then `►Name◄`, frame, `NN` / `--` (worn, none in pack), the class glyph of DS 0x1ae8 in RUNES.CH for this member's worn items, the 10-cell names of DS 0x1962 (`Leath Helm`), bar, arrow | handheld panel, `Chain Coif [equipped]`, long names | **IMPLEMENTED** (DOS panel; the roster picker was already DOS) |
| **Z-stats / inventory** | pages in window 1: stats (`♂ Lv-2 Avatar`, `Good Health`, `Str=..`), Arms (worn slots, short names), Provisions, four lists in the scroll frame with `-` separators, `(None owned!)`; `Status: ` in the console | handheld panel (14-px rows), `F`/`M` letters, long names | **IMPLEMENTED** (DOS panel, ♂/♀ glyphs, short names) |
| **(M)ix** | `►Reagents:◄`, **no frame**, ` NN NAME` (count '0'-padded, `Sp. Silk`, `Blk. Pearl`), bar over the whole row, the mark one reverse-video cell; the legend in the console | handheld panel, long names | **IMPLEMENTED** |
| **Shops** | console shops: lettered lists (`a...Dagger`, `A...Ginseng`, `a.........Keys`, the wine list), the questions (`What may I show thee?`, `Thy interest?`, `Is this thy need?`, `Deal?`…), a letter picks, Space leaves; the blacksmith's Sell is the `Arms` window (roster box, 4 rows `qq-Short Name`, status box below); the inn's `GUEST REGISTER`; healer/inn members on the roster | the handheld's shop panel (offers with prices, gold, log) in every phase | **IMPLEMENTED**: no shop panel; the lists, menus and deal questions printed on the console; letter keys (the tavern's per-town letters, the guild's `D`, the shipwright's F/S); `Arms` and `GUEST REGISTER` as DOS panels. **REMAINING**: the echoes and some connective lines the core never emits (§9) |
| **Other lists** | the roster picker `►Select:◄` in the band | `Select:` in the status caption row | unchanged (**REMAINING**, §9: the band row has no room above the T-Deck roster) |
| **Presentation** | one framed screen, 320 × 200 | the T-Deck 320 × 240 layout scaled; Classic Framed ring only when chosen | **measured and explained** (§7); **MOCKUPS** of better compositions; no code change |

## 4. What changed (IMPLEMENTED)

Shared code, every path gated by `platform_dos_interface()` or a profile hook that is null on the
T-Deck:

- `device_ui_views.h`: `DeviceDosPanel` (banner, layout, 9 × 16 cells, RUNES.CH and reverse-video
  masks, arrow) and pure helpers: `draw_list_frame`, `print_list_row` (`dos_list_row`), the minimal
  scroll, the arrow choice. `DeviceSelectionView` gains one pointer, `dos`.
- `tdeck_board.cpp`: `Board::draw_dos_panel()` paints x 182..319 × y 0..87 at the DOS geometry (§2)
  from the panel's cells (IBM.CH / RUNES.CH from the packs), the banner with the band's brackets, the
  arrow notch; the two-box layout for the `Arms` window. `show_alpha()` draws it instead of the roster
  and status rows and goes on to draw the console as in the world; leaving it restores the boxes.
  The cache is a heap allocation made on first use (the T-Deck's `.bss` keeps a pointer).
- `alpha_runtime.cpp`: `compose_dos_panel()` (Use, Ready, Mix, Z-stats pages and lists, `Arms`,
  `GUEST REGISTER`); Use rows in id order; `present_dos_shop_event()` prints the console shops;
  `compose_shop_view()` returns no panel in Original; a developer harness hook `open_shop_for_test()`.
- Core: `PlatformProfile::dos_text` / `platform_dos_text()` (`DosText` tables);
  `ShopSession::recent_draws` (read-only record of the shop's own rand draws, no draw added);
  `shop_console_text()` (the DOS strings already compiled in `shop_tables.inc` /
  `shop_greetings.inc`, unused until now); `tavern_menu_letter()`; `UiSession` shop keys in Original
  (letters only, Space leaves, the tavern's letters, `Arms` keys).

Windows only: `app/dos_text.{h,cpp}` reads the DOS tables from the pack's `data.json` (the extractor's
dump of the player's DATA.OVL, identity-locked with the pack: potion colours, reagents, spells, the
(U)se entries, DGROUP literals by address); the (R)eady name and glyph tables, which `data.json`
does not carry, are generated into the build tree from the copies the repository already tracks for
the web port (`cmake/DosTables.cmake`; no new game text is tracked). The desktop profile serves them.

## 5. Tests

| Gate | Result |
|---|---|
| New autotest facts | `right_column` **reads the presented panel's pixels** (`dos_box`, `dos_split`, `tdeck_list`, `tdeck_shop`, `world`); `dos_panel`, `dos_panel_title`, `dos_panel_layout`, `dos_panel_row`, `dos_panel_bar`, `dos_panel_arrow`, `shop_panel`; `overlay_panel` now means the handheld's right-column panel; verb `shop TYPE` (Developer) opens a live keeper's shop through `begin_shop` |
| `scenario_original_dos_panels` (new) | R / U / Z / M and the reagent, blacksmith (Buy, Sell), guild and healer shops in Original: the panel kind on the pixels, the rows, the bar, the arrow, the console lines |
| `scenario_enhanced_panels` (new) | the same keys in Enhanced: the handheld's list / shop panels, never a DOS panel |
| Updated | `original_interface` and `original_arena` asserted `overlay_panel 1` -- the handheld panel -- for Ready / Z-stats in Original; they now assert the DOS panel (`dos_panel 1`, `right_column dos_box`) |
| Mutants (one edit each, built, run) | 8 / 8 killed: lists or Z-stats back on the handheld panel; the shop panel back; **views right but pixels wrong** (only `right_column` sees it); no console lists; potions-first order; device names; arrow keys in the shop lists (killed after the scenario gained "RETURN leaves the reagents") |
| Windows ctest (MSVC, the real exe) | **33 / 33** (31 + the two new scenarios); `native/core/origdos-windows-ctest.log` |
| Host suite (GCC 16, serial) | **207 / 207**; `native/core/origdos-host-ctest.log`. The first full run (`origdos-host-ctest-first.log`) had 206: `spp1_runtime` B1 drove the tavern in Original with the handheld's fixed `d` / `t`; the pin now uses the tavern's DOS letters in Original (B-shops P5) and keeps `d` / `t` on the T-Deck |
| `win_dos_runtime` W7 / T7 (new, the real Board's pixels) | W7: with a DOS profile, (R)eady composes and DRAWS the 1988 panel (band y 3, rule y 7, console rule y 87, none of the selector's rules), the console keeps `Item: ` and its cursor, ESC restores the boxes; T7: the T-Deck draws its selector list, no DOS panel. 25 / 25 checks |
| Mutation log | `native/core/origdos-mutants.log` |

## 6. T-Deck and Enhanced

- **Code audit.** The T-Deck never installs a profile (`dos_text` and `dos_interface` are null), so
  `compose_dos_panel()` returns nothing, `DeviceSelectionView::dos` stays null and the Board takes its
  old branches; `present_dos_shop_event()` and the new `UiSession` shop rules test
  `platform_dos_interface()` first; `ShopSession::recent_draws` only records values the shop already
  drew; `shop_console_text()` / `tavern_menu_letter()` are reachable only from those Original paths.
- **Enhanced**: `scenario_enhanced_panels` and the existing Enhanced scenarios pass unchanged.
- **Host suite, firmware and sizes**: filled in at §6.1.

### 6.1 Results

| Check | Result |
|---|---|
| T-Deck differential (method `native/core/tools/spp2_differential/README.md`; base = a detached worktree at `bf3480e3`, head = this branch; the probe registered temporarily in both and removed) | **byte-identical**: unpaced 63,635 lines, SHA-256 `0e53ca58…2000` on both; paced `f01c2803…e828` on both. Positive control, the DOS profile in both trees: 159 walks differ (A 21, Aact 21, B 10, C 59, D 40, E 1, H 6, I 1) -- the probe sees Original's changes, and the default profile shows none. `native/core/origdos-tdeck-differential.log` |
| Firmware (ESP-IDF 6.1, the SPP2 `sdkconfig`, `-Werror`), against SPP2's `build-spp2` map | builds with 0 warnings; image 1,089,136 B vs SPP2 1,074,048 B (+15,088 B: flash `.text` +13,324, `.rodata` +1,744); internal RAM: DIRAM `.bss` +16 B, `.data` +32 B, IRAM unchanged; app partition 16.9 % free; `check_app_budget.py` passes; Launcher allocation unchanged at 1088 KiB (24,977 B to the next step). The growth is the Original-only code, compiled into every image and never run on the device (runtime gates, as for D22-D25). Embedded identity `eeffb8501f31-dirty` (the code commit; the record's docs were not yet committed). `native/targets/tdeck/origdos-fw-*.log`. Stack usage was not re-measured. **Not flashed.** |
| Host suite | 207 / 207 (§5), including every panel golden and the A4-UI preservation tests |
| Enhanced | `scenario_enhanced_panels`, `enhanced_interface`, `list_cast_enhanced`, `quicksave_enhanced`, `spp1/spp2_enhanced` pass |

## 7. Presentation — what Classic Framed changed, and why the playtest did not show it

**What D21 (Classic Framed, `18e17ea9`, in SPP2) changed** -- presenter only, the picture's pixels
untouched: a ring outside the 320 × 240 picture (4u black, 1u white rule, 2u band, corners cut) when
both margins hold 7u; `Window N×` = (320+16)N × (240+16)N when framed; the automatic scale from the
display's content scale; the Scaling row cycling Framed / Whole pixels / Fit; Fit's pixel-art sampler.

**Why the playtest showed no frame.** The player's `windows.json` reads
`"scaling":"Integer","windowScale":3` -- written by the first Windows builds, before D21. A saved
scaling is honoured, so the SPP2 exe ran **Whole pixels at 3×, a bare 960 × 720 window: no ring at
all** (the log line in §1). WINDOWS_PORT_PLAN §9 already listed this (no migration to Framed was
decided). Settings → Scaling → Framed shows the ring.

**Why it would still look mostly black.** Read back from the packaged exe (offscreen driver, the
same scene, `C:\Dev\uvw-original-dos-evidence\presentation\`):

| Configuration | Window | Picture | Black letterbox | Ring |
|---|---|---|---|---|
| The playtest (Integer 3×) | 960 × 720 | 100 % | 0 % | **0 %** |
| Framed 3× | 1008 × 768 | 89.3 % | 6.7 % | 4.05 % |
| Whole pixels, 1920 × 1080 | | 59.3 % | 40.7 % | 0 % |
| Framed, 1920 × 1080 | | 59.3 % | 38.1 % | 2.69 % |
| Fit, 1920 × 1080 | | 75.0 % | 25.0 % | 0 % |

The ring is 1u of white and 2u of band: at 3× a 3-px white line and a 6-px blue line. Inside the
picture, the T-Deck layout keeps a black glass strip under the map (x 0..180 × y 180..239 = 10,860 px,
14.1 % of every frame) and a 152-px-tall console that is mostly black; neither is touched by a
presenter-only change, by design (D21).

**MOCKUPS** (`C:\Dev\uvw-original-dos-evidence\mockups\`; real captures, the frame's own band blue and
white, whole-number scales only, no new art):

- **A** -- the current picture at 4× with the ring, the letterbox filled with the band and a white
  rule at the screen edge: one unified frame with no code outside the presenter.
- **B** -- Original in the DOS 320 × 200 composition: the map and the right column as they are, the
  band continued under the map instead of the glass, the console's 14 newest rows; 5× on 1920 × 1080
  (77 % of the screen instead of 59 %), band-filled margins.
- **C** -- the playtest's 960 × 720 window as it is, beside B at the same window size.

B needs a 320 × 200 Board layout in the shared Board for Original (a `BoardLayout`, new goldens);
A is a presenter change. Neither is implemented; both are proposals.

## 8. Test package

- **File**: `native/targets/windows/dist/UltimaV-Native-Windows-x64-ORIGDOS1-e8cb8ba5-test.zip`
  (this worktree), 2,304,467 B, SHA-256
  `1dde51abe33f78f4ccf84c3e053b8a52ab36c772ce60b4ee8217ac0599e809da`. A new name: the SPP1 / SPP2
  test ZIPs and the Windows worktree's development ZIP are untouched.
- **Built** by `tools/build-windows.ps1 -Config Release -Test -Package -PackageName
  UltimaV-Native-Windows-x64-ORIGDOS1-e8cb8ba5-test` from `e8cb8ba5` with a clean tree; BUILD-INFO
  `Source: e8cb8ba5… (windows/original-dos-interaction)`; the exe logs `git e8cb8ba50ba5`. The
  packaged build passes **33 / 33** (`native/core/origdos-win-package.log`).
- **Smoke**: the packaged exe, headless on a scratch data folder, ran the U / R / Z / M script with 0
  failures and logged `DOS_TEXT tables loaded`; its Ready, Use and Mix panels are pixel-identical to
  the tested build's (the Z page differs only in the new character's rolled Dex).
- **To see it as tested**: unzip into a new folder; the packs are the same as before (copy the three
  `openu5-*.bin` or run `Prepare-Game-Data.cmd`). The player's existing `windows.json` keeps
  `"scaling":"Integer"`; Settings -> Scaling -> Framed shows Classic Framed's ring (§7). Nothing here
  changes that file.
- **Not a release.** No person has played it.

## 9. Remaining (recorded)

1. Shop text the core does not emit: the Y/N and letter echoes (`Yes`, `No`, `a`), some connective
   lines (`What else?`, the inn's rate line, the shipwright's list record), the tavern rounds charged
   at once in DOS (P4), and D-104's semantics -- ledger D-105 P1 / D-104; they need the core's own
   DOS transcript. *(ORIG-DOS2, §11: the core's DOS transcript now prints all of the text, and P4 is
   done; D-104's semantics remain.)*
2. `►Select:◄` stays in the status caption row: the T-Deck roster starts at y 4, the band has 2 rows.
   B (§7) would give it its DOS place.
3. The DOS box is 124 px wide inside, the T-Deck's 135: the window sits 4 px right of DOS's.
4. The Arms page's title underline (0xfe) is not drawn.
5. The console stays the T-Deck's 22-column compact face (DOS: 16 IBM.CH columns); the Mix legend
   still spells `Arrows`.
6. Home / End in the item lists (DOS item_page_controller has them) are not mapped on the desktop.
7. No live DOS capture of a shop (static evidence only). The attempt -- the party stood beside
   Moonglow's reagent seller in the Windows build, exported as a PC save and loaded in DOS -- met
   `Nobody's here!`: the PC export does not carry the town's people (its EXPORT.TXT says so) and DOS
   repopulates a town only on entry.
8. No person has played this build; no T-Deck was flashed.

## 10. Commits

| Commit | What |
|---|---|
| `eeffb850` | The 1988 right panel, the console shops, the DOS name tables, the tests |
| `e8cb8ba5` | This record, the fidelity matrix and plan updates, the logs |
| (this commit) | The test package's record (§8) |

## 11. ORIG-DOS2 — the console shops' DOS transcript (2026-10-14)

**Branch** `windows/original-dos-shops` · **worktree** `C:\Dev\uvw-dos-shops` · **base** `60e7c647`
(ORIG-DOS1's last commit) · **not pushed, merged, published or flashed.** The frozen branches
(`parity/shared-preservation-1`, `parity/shared-preservation-2`, `main`, `windows/initial-port`) and
ORIG-DOS1's branch were not written to.

§9 item 1 left the shops' console text half done: ORIG-DOS1's presenter printed the lists and the
questions, but the Y/N and letter echoes, the connective lines and the variant-picked forms belong
to the core, which owns every draw (B-shops.md §4: a frontend cannot reconstruct them). This batch
follows B-shops recommendation 11: the core prints the 1988 transcript itself.

### 11.1 What changed (IMPLEMENTED)

- **Core, `ShopServices::dos_transcript`** (default `false`; copied into `ShopSession` at
  `begin_shop`). With it the shop prints B-shops §3's sequences from the strings `shop_tables.inc` /
  `shop_greetings.inc` already compile (DATA.OVL DS): every `Yes` / `No` / `Yes!` / `No!` echo, the
  letter echoes (`a`, `b` at the blacksmith and the guild, the tavern's own letters, `F` / `S`,
  `R` / `L` at the inn), the lists (blacksmith, reagents, guild, wine), every question, `What else?`,
  `What else, then?`, `"Anything else, then?` / `sir?` / `milady?`, `"I thank thee!"` +
  `"Anything else?` after a reagent purchase, `What else, m'lady?`, the inn's rate line before
  `Wilt thou take it?`, the room's `"I am sorry, ... no room available."`, `Zzzzzz....`, `Morning!`
  and `has passed away.`, the bill read out at a pickup, the shipwright's records 119 / 117 / 118 /
  126 / 123 / 124 / 122, the healer's echoes (`Healing`, `Nothing`, `No one`) and record 173, the
  tavern's announcement (`That will be N gold for the two..six of ye, sir.`), the drunk warning,
  the rations' afford-only / scraps / `Thou hast neither gold nor need!`, the rumour's question and
  its retry, records 84 / 85..88 / 91, the sell flow's prompts, `Done!`, the ammunition and
  no-value refusals and its `Good-bye`. The core's own result strings (`It is done.`, `She awaits
  thee at the dock!`, `It shall be done.`, the rations' `Anything else?`) are not shown in that mode.
- **The 1988 console's cursor, `DosConsole`** (`shop_orchestration.h`): kernel `print_string`
  0x1850 (its segment loop, the cut at the last space that fits, the eaten space and the owed line
  feed of 0x19c7, the long-word break of 0x1981) and `putchar` 0x16ba (the wrap past column 15) on
  the 16-column console (re/notes/printstr-1850-derivacion.md). Each DOS call -- a string or a
  record is a `print_string`, a single character a `putchar` -- goes through it, so the transcript
  arrives laid out in DOS's own rows, and the branches SHOPPES takes on the cursor are taken on it:
  the greeting's `"\n\n:"` / `"\n:"` / space (0x01c6), the reagent pitch's `\n:` (column > 12), the
  blacksmith row's line feed unless at column 0, the tavern menu's line feed past column 14, the
  farewell's line feed before `says $.`. A typed rumour word or ration count moves the cursor too.
- **The transcript reaches the console as stream Messages** (`GameEvent::stream`): `UiSession`
  appends them where the last one stopped (`append_stream`, `UiTextStreamOpen`), the live prompt
  continues an open row (the count typed after `thou like?" `), and a typed answer stays on its row.
  With the transcript `UiSession` prints no `Shop / Keeper, proprietor` heading and no prompt of
  its own for the rumour and the rations, and the rations' Result event no longer reopens the count
  it has just taken (the count the player typed, then the next key, went into a fresh number entry:
  found by the scenario, see §11.4).
- **Keys (Original)**: `Anything else for thee?` and the shipwright's "anything else" take Y or N
  only (Space was N; B-shops P5, SHOPPES2 0x07a1 / 0x0862); ESC is read again at the wine list
  (0x02c8).
- **Windows Original sets the transcript and drops the staged services** (`sync_shop_profile()`,
  before every command and the harness's shop): the tavern's food round and house drink are
  charged at once as in 1988, with no question (B-shops P4); the room's and the rations' prices are
  no longer staged in the session (the transcript prints them, as DOS does, in the pitch). `AlphaRuntime::present_dos_shop_event()` now prints nothing (the core
  prints it all); ORIG-DOS1's presenter-only core API went with it (`ShopConsoleText`,
  `shop_console_text()`, `ShopSession::recent_draws`).
- **Windows harness**: `shop TYPE TOWN` (another town of the type: the wine tavern, 19), `give gold
  N` / `give reagent SLOT N`, and `expect rows` / `no_rows` (the transcript as console rows, joined
  `" | "`, scrolled-out rows included).

Files touched outside the shop code (for the merge with the 320 × 200 screen work): `movement.h`
(`GameEvent::stream`); `ui_session.{h,cpp}` (`append_stream`, `UiTextStreamOpen`, the live prompt's
join in `for_each_wrapped_line`, the shop branch of `dos_commit_answer`, besides the shop event and
key handling); `alpha_runtime.{h,cpp}` (`sync_shop_profile()`, called at the top of `command()` and
in `open_shop_for_test()`; `present_dos_shop_event()`'s body); the Windows harness
(`app/application.cpp`, `app/autotest.{h,cpp}`). `tdeck_board.cpp`, `compose_dos_panel()`, the
console and banner code and `presentation_layout` were not touched.

Nothing changes on the T-Deck: it never sets the flag (no stream Message, no `DosConsole` call) and
keeps `transactional_services=true`; every new `UiSession` path needs a stream Message or the
session's flag, except the two key rules above, which sit inside `platform_dos_interface()`.

### 11.2 Evidence for the sequences

- **B-shops.md §3** (the RE pass, DS offsets) and the web port's `game/src/ui/shop-console.ts`.
- **DATA.OVL**: every DS string the transcript prints was read back from the player's DATA.OVL
  (`fileoff = DS + 0x10`) and matches `shop_tables.inc` / `shop_greetings.inc` byte for byte.
- **SHOPPES*.OVL**, disassembled with the repository's `re/tools/batch45_disasm.py` where B-shops
  is silent or ambiguous: the tavern menu's `'"'`, `' '`, then a line feed past column 14
  (SHOPPES2 0x06c0-0x06da); the two line feeds of the food round and of the drink are the cost
  helper's (0x0000-0x005f), printed before the `'W'` test, so the wine list follows `W\n\n` (the web
  port omits them there); the unknown rumour word reprints the question from `Of what wouldst`
  (0x0591 -> 0x052a); the inn's room check (0x002c) prints `\n\n` for both Rest and Leave; the inn
  loop (0x08b4): Leave's Avatar asks again (0x0351 -> 0x0480 -> 0x0301), the night, the lone Avatar
  (record 191), the morgue (192) and the guards (193) end the visit with farewell(-1), i.e. none.

### 11.3 Tests

| Gate | Result |
|---|---|
| `shop_dos_transcript` (new host test, `native/core/tests/shop_dos_transcript_test.cpp`) | **125 / 125 checks** (`native/core/origdos2-green.log`). Synthetic shop data and `R<n>` records (no game text): `DosConsole` against the kernel printer's cases; blacksmith Buy (echo, chatter, list, letter, `No`, `Anything else, then?`, `Yes`, `Sold!`, `milady?`/`sir?`, farewell), Space at Buy-or-Sell; Sell (prompt, offer, `Deal?`, `Done!`, SELLMORE, no value, `No`, good-bye with no farewell, ammunition, nothing to sell); reagents (the 99 cap with no pitch, `What else?`, `I thank thee!` / `Anything else?`, N at the greeting); guild (`D`, letters, `What else, then?`, `Sold!` / `m'lord`); tavern (rounds charged at once, rumour with retry, price, gossip; wine list, Space at it, a wine; rations bought and `Hrumph.`; the drunk warning, Y and N); healer (powers, `Healing`, the offer, `Yes`, no need, `No one`, `Nothing`); inn (Leave, the Avatar, the rate line, `I thank thee.`, the register's `Who will check out?` and `No one`, pickups, the room's price, the night with no farewell, N at the room); shipwright; horses (all three greeting spacings); the default profile prints no transcript; `UiSession`'s stream, a full block, and the rations' count. The variants are read off the shop's own draws (its rng trace); the blacksmith and reagent visits check the draw count, eight of the shops that no line or result string escapes the transcript |
| RED first | `native/core/origdos2-red.log`: (1) against the API stubs with `DosConsole` implemented, 78 of 110 checks failed (every sequence; one failure is a case of my own hand-laid expectation, corrected before GREEN); (2) the rations' count reopened by its Result, 2 checks |
| Mutants (one edit each, rebuilt, run, reverted) | **8 / 8 killed** (`native/core/origdos2-mutants.log`): no `Yes` echo; the greeting's spacing; `DosConsole`'s owed line feed; the reagent pitch before the cap; the result strings shown again; the stream starting a new row; the inn's rate line; a farewell after the night. Windows: Original back on the staged tavern rounds -- **killed** by three scenario checks (`native/core/origdos2-windows-mutants.log`). The first host pass let two mutants survive (the greeting's single space and the owed line feed had no case); the horse visit and the trailing-space case were added for them |
| `scenario_original_dos_panels` (extended) | line by line on the real exe, the DATA.OVL strings only (the records, the keepers' names and the rand-picked forms vary or are game text): blacksmith Buy + Sell, reagents with Ginseng at 99, the guild with `D`, the tavern (round and house drink charged at once, Space not N, the rumour), the wine tavern, the rations (`2`, then `0` = `Hrumph.`), the healer (Space read again at "any other way"), the inn (two Leaves, the register, both pickups, the night, no farewell). ORIG-DOS1's own `console` checks for `Is this thy need?`, `What is the nature of thy need?` and `Who needs my aid?` now read them across DOS's row breaks (`this thy need?`, `"What is the | nature of thy | need?"`) |
| Windows ctest (MSVC, the real exe) | **33 / 33** (`native/core/origdos2-windows-ctest.log`) |
| Host suite (GCC, w64devkit) | **208 / 208** (`native/core/origdos2-host-ctest.log`): the 207 of 60e7c647 + `shop_dos_transcript`. In the serial run 207 passed and `typescript_item_fixture_drift` (Node, no shop code) crashed with a SEGFAULT while an ESP-IDF build was running beside it; re-run alone it passes (appended to the log). The baseline at 60e7c647 in this worktree was 207 / 207 |

### 11.4 T-Deck, firmware

| Check | Result |
|---|---|
| T-Deck differential (`native/core/tools/spp2_differential/README.md`; base = `C:\Dev\uvw-shops-base` detached at `60e7c647`, head = this tree; the probe registered temporarily in both and removed) | **byte-identical**: unpaced 63,635 lines, SHA-256 `0e53ca58…2000` on both; paced 63,620 lines, `f01c2803…e828` on both (the same figures as ORIG-DOS1's §6.1). Positive control, the DOS profile in both trees: **77 walks differ, all in family C** (the shops), the greetings now laid out in DOS rows with their gate spacing. `native/core/origdos2-tdeck-differential.log` |
| Firmware build and size diff | **not run here**: by agreement with the session folding this work into `windows/original-dos-interaction` (the 320 × 200 Original screen, `ccb09116`), the firmware build, its size diff, the T-Deck differential and the packaging are run once on the combined branch. (A first build here hit an internal compiler error in ESP-IDF's own `esp_lcd` while another build was running, and was stopped.) |
| The rations' count on the T-Deck / Enhanced | **not changed**: the same Result event reopens their number entry once after a purchase (the player backs out of it). The fix is behind the transcript flag only, to keep the T-Deck byte-identical; recorded as a follow-up |

### 11.5 Test package

None from this branch (§11.4): the package is made once, from the combined branch.

### 11.6 Remaining (recorded)

1. **D-104, the shops' semantics (B-shops S1–S19), unchanged** -- the transcript prints what the
   core does. Visible in it: after an inn night, the lone Avatar, the morgue and the guards the
   core still draws its farewell's variant (S7; Original prints no farewell text, the draw is
   kept so both platforms stay one game); an unpaid rumour goes on to "Anything else" (S15; DOS
   ends the visit); the ship flags (S5) -- records 120 / 121 / 125 are never printed, and no drain
   after a ship (S4); the drunk counter is per visit (S13); a negative ration count is `Hrumph.`
   (S16); the buyer's INT is member 0's (S1). In Original only, a companion left at the inn marks the
   visit as a purchase for the farewell's table (S11: text only, same draw).
2. The shipwright does not reprint record 119 for a key other than F, S or Space (DOS does).
3. The sell offers' `&` is the device's long name; DOS uses its override table (DS 0x3cce: `Cloth
   suit`, `Two-Handed Sword`, ...).
4. The rumour's and the rations' entries: ESC ends them as an empty answer / no rations (DOS's
   getstring and getnum erase on ESC and end only on RETURN).
5. On this branch the console stays 22 columns wide: the shops' rows are DOS's 16-column rows
   (laid out by the core), every other line still wraps at 22 (§9 item 5). `ccb09116`'s 16 × 13
   console (`platform_dos_screen()`) is the one these rows are made for.
6. The shop's first cursor column is taken as 0 (the Talk echo's row closed).
7. Not on the scenario (host-tested): the horse seller, the shipwright, a paid heal, the drunk
   warning. Not tested anywhere: the healer's charity (record 173) and Skara Brae, the scraps
   (record 90), afford-only, records 191 / 192 / 193.
8. The T-Deck / Enhanced rations re-entry (§11.4).
9. No firmware build, size diff or test ZIP from this branch (§11.4).
10. No person has played this build; no live DOS shop capture (§9 item 7); no T-Deck was flashed.

### 11.7 Commits

| Commit | What |
|---|---|
| `4de1d243` | The core's DOS transcript (`dos_transcript`, `DosConsole`), `UiSession`'s stream, the Original keys, `sync_shop_profile()`, the removed presenter, the tests and the scenario |
| (this commit) | This record, the fidelity matrix, the logs |
