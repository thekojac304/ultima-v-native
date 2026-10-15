# Windows Original — the 1988 game's own 320 × 200 screen (ORIG-DOS2)

**Branch** `windows/original-dos-interaction` · **worktree** `C:\Dev\uvw-original-dos` · **base**
`60e7c647` (ORIG-DOS1 HEAD) · **date** 2026-10-14 · **not pushed, merged, published or flashed.**
The frozen SPP1 / SPP2 branches, `main` and the Windows worktree (`windows/initial-port`) were not
written to.

The owner chose mockup B of the ORIG-DOS1 record (`WINDOWS_ORIGINAL_DOS_INTERACTION.md` §7) as
Windows Original's default: the DOS game's 320 × 200 screen composition, with Enhanced keeping the
handheld's 320 × 240 one. Mockup B was a crop of the handheld frame; this batch **recomposes** the
screen: every element is drawn where ULTIMA.EXE draws it. It also removes the startup lines from
Original's console, finishes the DOS shop transcript (§6), and leaves the T-Deck unchanged.

| Word | Meaning |
|---|---|
| **IMPLEMENTED** | In this branch, built, tested (§7); in the test ZIP (§10). |
| **REMAINING** | Recorded, not changed here; the reason is given. |

## 1. The screen (IMPLEMENTED)

The geometry is ULTIMA.EXE's own frame routine, 0x63ac..0x64f7, as the web port's faithful skin
transcribes it (`game/src/skin/fiel/frame.ts` `FRAME_FILLS` / `FRAME_GLYPHS` / `FRAME_SEGMENTS`; the
console's text window from INTRO.OVL 0x0d1a), checked against DOSBox-X captures of the player's own
install (`C:\Dev\uvw-original-dos-evidence\dos-1988\`, kept out of the repository):

| Part | Where (320 × 200, 8 × 8 cells) |
|---|---|
| Band (EGA blue) | y 0..6 across; x 0..6 and x 185..191 down to y 191; y 185..191 left of x 192; the strip x 313..319 down to y 87; the bars between the panel's boxes, y 57..63 and y 80..87 |
| Corners | IBM.CH 0x7b / 0x7c / 0x7d in the band's blue at cells (0,0), (39,0), (0,23) |
| White rules | the map's box x 7 / x 184, y 7 / y 184; the roster box x 191 / x 312, y 7 / y 56; the status box y 63 / y 80; the console's x 191 (y 87..191) and y 87 (x 191..319) |
| Map | the 176 × 176 viewport at (8,8), **whole** (on the handheld screen the sky and wind strips cover its top and bottom 9 rows) |
| Sky band | row 0: sun and moons in RUNES.CH, cells 6..17 (x 48..143), `►` at x 40, `◄` at x 144 |
| Wind band | row 23: `►Calm  Winds◄` from col 6 (ULTIMA.EXE 0x2ecb; the direction is a 5-cell field, so the caption is always 11 cells) |
| Dungeon bands | `L#` on row 0 and `Dir:  East` on row 23, centred on the map's centre (x 96), as `dng_draw_panel` |
| Panel (text window 1) | the roster on rows 1..6 (`name`, `→` on the active member, HP in 4, status), F/G on row 8, the date centred on row 9; `►Select:◄` on the band above the roster while a member is picked; the lists (Use, Ready, Z-stats, Mix, Arms, the guest register) as ORIG-DOS1's DOS panel, now at the DOS geometry exactly (rules x 191 / 312, col 0 at x 192) |
| Console (text window 2) | cols 24..39, rows 11..23: 13 rows of 16 IBM.CH cells from (192, 88), white, filling upward from the bottom row; the echo bullet is glyph 0x02 drawn as `draw_box_edge` draws it (blue with white edges to the tip at col 5); the wait cursor after the newest row. Row 24 (y 192..199) is black, as in DOS. (ORIG-DOS1 §2 said rows 11..24; the window ends at row 23.) |
| `►◄` ends | glyph 0x02 / 0x01 in the band's blue with the two white lines of `draw_box_edge` 0x4c2a / 0x4cce from (x, y) and (x, y+7) to the tip; the notch is black over all eight rows, rules included |

**Checked against the 1988 captures.** Every pixel of our world screen outside the parts that hold
game content (the map, the band notches' insides, the panel's window rows, the console) was compared
with the DOS world capture: **0 differing pixels**. The same for the Ready list (the box layout):
**0**. The Use list differs only at the arrow notch, because the 1988 save lists seven items (a
scroll arrow) where the new character has two (none) — content, not frame. (`chromediff.py` in the
session scratchpad; the same comparison runs as the test `D3a` against the ROM tables, §7.)

**How it is built.** `native/targets/tdeck/main/board_dos_layout.cpp` is a part of `tdeck::Board`
that only the desktop target compiles: `native/targets/windows/CMakeLists.txt` lists it and defines
`OPENU5_DESKTOP_DOS_LAYOUT` (PUBLIC, so every file agrees on the class's layout). `tdeck_board.cpp`
dispatches `show_alpha` (except the Developer screen), `show_camp_viewport`, `fill_bed_viewport` and
`refresh_bed_status_panel` to it under `#if`, and puts the ending's 320 × 200 page at row 0 instead
of row 20; the firmware's preprocessed source is what it was. The world's panel is composed as a
`DeviceDosPanel` too (the roster and status boxes), so one painter draws every right panel; each
part keeps a cache, as the handheld screen does. `Board::picture_height()` is 200 for the DOS screen
and 240 for the screens that stay the handheld's (the title and frontend menus, the System Menu, the
Developer screen).

**Who gets it.** `PlatformProfile::dos_screen` (null on the T-Deck) is "Original, and
`windows.json` does not say `"originalScreen": "Handheld"`" (`--original-screen handheld` for one
run). Enhanced is always the handheld's 320 × 240 screen. Switching the interface in Settings
switches the screen and reshapes the window at once.

## 2. Presentation: Framed, Whole pixels, Fit, fullscreen (IMPLEMENTED)

The presenter shows the panel's first `picture_height()` rows. `presentation_layout` takes the
picture's height: Framed and Whole pixels use `u = max(1, min(W div 320, H div 200))`, the ring is the
same 4u / 1u / 2u, Fit is the largest **8:5** rectangle. "Window N×" is shaped for the mode's screen
(a framed client of 336N × 216N, bare 320N × 200N); the 320 × 240 frontend shown in that window takes
the largest whole scale that fits there. F12 saves the 320 × 200 picture.

| Output | Framed | Whole pixels | Fit |
|---|---|---|---|
| Window 3× (1008 × 648 framed) | 3× 960 × 600 at 24,24, full ring | 960 × 600 bare window | 1008 × 630 (fractional) |
| 1920 × 1080 (fullscreen / maximised) | **5× 1600 × 1000** at 160,40, full ring | 5× | 1728 × 1080 |
| 2560 × 1440 | 7× 2240 × 1400, pillars | 7× | 2304 × 1440 |
| 3840 × 2160 | 10× 3200 × 2000, full ring | 10× | 3456 × 2160 |
| 1280 × 800 | 4× exactly, no room for a ring | 4× | 4× |

At 1920 × 1080 the DOS screen is shown at 5× where the handheld's 320 × 240 fits only 4× (1280 × 960):
the picture covers 77 % of the screen instead of 59 %. A 1920 × 1080 work area now holds Window 4×
framed (the handheld's 4× had to go bare).

## 3. The console's startup lines (IMPLEMENTED)

Original no longer prints `Ultima V Native for Windows 0.5.0-dev` and `Esc back  F10 menu  Alt+Enter
fullscreen` into the game's console: it opens empty, as the 1988 game does. The build stays
reachable: the System Menu (F10) shows `Ultima V Native 0.5.0-dev, git <hash>` under its title
(`PlatformProfile::about`; null on the T-Deck, whose System Menu is unchanged), the log's first line
names it, and the executable's version resource carries it. Enhanced still prints both lines.

## 4. What else changed with the screen

- The console is 16 × 13 on the DOS screen (`kDosConsoleColumns` / `kDosConsoleRows`): the session
  wraps and pages by it, so prompts break where DOS breaks them (`For how many` / `hours? (1-9)`).
- Console text is white, as in DOS (the handheld screen colours prompts cyan and combat red).
- The status box is DOS's: F/G and the centred date; the handheld's location caption, clock and
  `MOVE` are not drawn on the DOS screen.
- A list the runtime has no DOS form for would be shown in the DOS list frame under its title (none
  is reachable in Original today).

## 5. Enhanced, the handheld fallback and the T-Deck

- **Enhanced**: the same 320 × 240 screen, window and banner as before; `scenario_enhanced_interface`
  and `scenario_enhanced_panels` assert it.
- **Original on the handheld screen** (`"originalScreen": "Handheld"`): ORIG-DOS1's screen;
  `scenario_original_dos_panels_handheld` and `scenario_presentation` (`--original-screen handheld`)
  keep it covered.
- **T-Deck** (§8): nothing of the DOS screen is compiled into the firmware.

## 6. DOS shop transcript (IMPLEMENTED)

Done in its own session on `windows/original-dos-shops` (from `60e7c647`, commit `4de1d243`) and
cherry-picked here as `07edc45b` (no merge; one conflict, the Original scenario, resolved by taking
the shop commit's steps and renaming its `right_column` expectations for the DOS screen).

ORIG-DOS1 left the text the core did not emit: the Y/N and letter echoes, connective lines, the inn's
rate line, the shipwright's records, the tavern's rounds charged at once (B-shops P4). Now, with
`ShopServices::dos_transcript` (off by default, so the T-Deck and Enhanced are unchanged), **the core
prints the 1988 console sequences of `native/core/spp1-findings/B-shops.md` §3 itself**, from the
DATA.OVL strings `shop_tables.inc` already compiles: the `Yes` / `No` and letter echoes, the lists,
the questions and the connective lines (`What else?`, `"Anything else?` after a reagent, `Anything
else, then?`, `What else, m'lady?`, the inn's rate line, the shipwright's records, the tavern's price
announcement, the drunk warning), laid out in DOS's 16-column rows by `DosConsole` (a model of the
kernel's `print_string` 0x1850 / `putchar` 0x16ba, so the cursor-driven spacings are the binary's).
The core's own result strings are not shown in that mode. `Anything else for thee?` and the
shipwright's question take Y/N only; the tavern's rounds are charged at once (P4). ORIG-DOS1's
presenter-only shop text API is removed.

On the DOS screen the transcript's 16-column rows land on a 16-column console, exactly as DOS shows
them. Remaining (that session's record): the D-104 semantics (B-shops S1–S19), the shipwright's menu
not reprinted on another key, the sell offers' item names (DS 0x3cce), ESC in the rumour and ration
entries.

That session's record is §11 of `WINDOWS_ORIGINAL_DOS_INTERACTION.md` (cherry-picked as `98e26f43`,
with its logs `native/core/origdos2-{red,green,host-ctest,mutants,tdeck-differential,windows-ctest,windows-mutants}.log`,
taken on its own branch). Its tests: `shop_dos_transcript` (host, 125 checks, written RED first), and
`scenario_original_dos_panels` extended line by line through every shop on the real executable
(`expect rows`, the transcript's own rows); 8 / 8 host mutants and a Windows mutant (the tavern's
staged rounds) killed; its T-Deck differential identical unpaced and paced, the DOS profile differing
in 77 shop walks (the positive control).

## 7. Tests

On the integrated branch (the DOS screen and the shop transcript together):

| Suite | Result |
|---|---|
| Windows ctest (MSVC, `build-dos`) | **35 / 35** — 8 unit executables, 27 headless scenarios of the real exe (two new: `presentation_dos`, `original_dos_panels_handheld`); again in the packaging run (§10) |
| Host suite (GCC, `native/core/build-dos`, serial) | **208 / 208** (`native/core/origdos2-final-host-ctest.log`; `shop_dos_transcript` is the 208th) |
| Mutants of the DOS screen | 15 / 15 killed (§7.1) |
| Mutants of the shop transcript | 8 / 8 host and the Windows one (that session's record) |

A note on the host suite: GCC 16.2 (w64devkit) crashed (an internal compiler error) twice while
building at full parallelism on this 28-thread machine, and three tests failed once under `ctest
-j8` and passed when re-run alone. The recorded run is a `-j 4` build and a serial `ctest`, with no
failure.

New checks:

- `uv_virtual_panel_test` D0–D24, D3a, D8b, D9b: the real Board's DOS screen on the virtual panel — the
  frame against ULTIMA.EXE's tables at all 10,304 pixels outside the content (an independent
  transcription of `frame.ts`), the map whole, the sky and wind bands, the roster / F-G / date rows,
  the status box's right-aligned G and centred date, the console's bottom-up rows and its 16-cell wrap, a list's box with banner, bar and arrow notch,
  the bed and camp draws, an ending page at row 0, the frontend at 240 rows, layout switches back and
  forth reproducing the screen pixel for pixel.
- `uv_presentation_test` S1–S12: 320 × 200 layouts worked by hand for 11 outputs × 3 scalings, every
  output 0..1100 × 0..760 and a grid to 7680 × 4320 against the layout rules (944k sizes), frames
  drawn from the layout pass the frame checker and a picture taken one panel row off is caught,
  Window N× plans shaped for 200 rows, the first-run default, client scales, the shape per mode.
- `scenario_presentation_dos`: the real executable — the frontend at 2× in Window 3× framed for
  200 rows, the DOS screen at 3×, `expect dos_screen frame` (the frame's own pixels) and the bullet's
  shape, fullscreen, drags to 1920 × 1080 / 2560 × 1440 / 1280 × 800 / 1366 × 768, Ctrl+= / Ctrl+-,
  Whole pixels and Fit from the System Menu, the interface switch reshaping the window to 1008 × 768
  and back; every `window_layout` reads the presented window back and compares it colour for colour.
- The Original scenarios run on the DOS screen (`right_column screen_box` / `screen_split`, read from
  the presented pixels; 16-column console text); `new_game_original` asserts the empty console and
  the System Menu's build line; `enhanced_interface` the handheld screen and its banner.
- Mutants: §7.1.

### 7.1 Mutants

Fifteen one-edit mutants of the new code, each built and run against `uv_virtual_panel_test`,
`uv_presentation_test` and the scenarios `presentation_dos`, `original_dos_panels`,
`new_game_original`, `enhanced_interface`, `original_interface` (`native/core/origdos2-final-mutants.log`).
Planning them showed two gaps, closed before the first round: nothing compared the whole frame (D3a)
or looked at the drawn console's wrap (D9b). The round then killed 14; M14 (the date not centred)
survived because no check looked at the date's cell. D8b was added and M14 re-run: **15 / 15
killed**, and the restored source passed. (The message of `f5361508` counts the wrap gap as a
round-one survivor; it was found while planning and closed before the round.)

| # | Mutant | Killed by |
|---|---|---|
| M1 | Original shows the handheld screen (the Board's DOS layout never set) | new_game_original, presentation_dos, original_dos_panels, original_interface |
| M2 | the presenter shows the panel's 240 rows on the DOS screen | presentation_dos |
| M3 | the window keeps the handheld's 240-row shape on an interface switch | presentation_dos |
| M4 | the console wraps at 22 cells on the DOS screen | uv_virtual_panel_test (D9b) |
| M5 | Original prints the build line and key legend again | new_game_original |
| M6 | the separator band one pixel left | uv_virtual_panel_test (D3a) |
| M7 | the band ends without `draw_box_edge`'s corner pixels | presentation_dos, uv_virtual_panel_test |
| M8 | Window N× keeps the 240-row aspect | presentation_dos, uv_presentation_test |
| M9 | F12 saves the whole 240-row panel | presentation_dos (the BMP's size) |
| M10 | the frontend cut to the DOS screen's 200 rows | presentation_dos, uv_virtual_panel_test (D19) |
| M11 | a list's box not joined across the bar | original_dos_panels, original_interface |
| M12 | the console bullet is the handheld's long one | presentation_dos |
| M13 | the wind caption centred instead of at col 6 | uv_virtual_panel_test (D7) |
| M14 | the date not centred in the status box | uv_virtual_panel_test (D8b) |
| M15 | the map clipped like the handheld's (top 9 rows) | uv_virtual_panel_test (D5) |

## 8. T-Deck

**By construction.** The DOS screen's code is in a file the firmware does not compile, and every
line of it that `tdeck_board.cpp` / `tdeck_board.h` reaches is inside `#if
defined(OPENU5_DESKTOP_DOS_LAYOUT)`, which only the desktop target defines. The shared hunks the
firmware does compile, one by one:

| Hunk | On the T-Deck |
|---|---|
| `PlatformProfile::dos_screen` / `dos_screen_context` / `about`, `platform_dos_screen()` | null hooks; `platform_dos_screen()` is false |
| System Menu root: `subtitle = about` when set | `about` is null: no subtitle, as before |
| `AlphaRuntime::initialize`: the two banner lines only when `!platform_dos_interface()` | the T-Deck has no `dos_interface` hook: both lines, as before |
| `refresh_session_context`: 16 × 13 when `platform_dos_screen()` | false: the old geometry branches |
| `device_ui_views.h`: `kDosConsoleColumns` / `kDosConsoleRows` | constants only |
| `tdeck_board.cpp` ending page / fizzle: `top` is `kEndgameTop` under `#else` | the same constant, 20 |
| The shop transcript (§6; that session's record §11.4): `ShopServices::dos_transcript`, `UiSession::append_stream` and the stream flag, `GameEvent::stream`, `AlphaRuntime::sync_shop_profile` | the flag stays off on the T-Deck (`sync_shop_profile` sets it only under `platform_dos_interface()`); the presenter-only shop text API ORIG-DOS1 added is removed |

**Differential run** (`native/core/tools/spp2_differential`, base `60e7c647` in a detached worktree
`C:\Dev\uvw-dos2-base`, both trees built with the same GCC, the probe hooked in and removed again),
on the integrated branch: 1,052 walks / 8,430 steps / 63,635 lines, **byte-identical** unpaced
(SHA-256 `0e53ca58…2000`) and paced (`f01c2803…e828`) — the same hashes as the SPP2 and ORIG-DOS1
records. With the DOS profile (the positive control) 77 walks differ, all of them shop walks (family
C, the shop transcript), and the head's hash `e727b576…168d` is the shop branch's own: the DOS
screen adds nothing there. (Before the shop commits were picked, the DOS-profile run was identical
too: the host fixture does not run the production `initialize()`, where the banner is printed, and
the probe's profile has no `dos_screen` hook.) Log: `native/core/origdos2-final-tdeck-differential.log`.

**Firmware** (ESP-IDF 6.1, the same sdkconfig; head built in `native/targets/tdeck/build-dos`, base
`60e7c647` built the same way in the base worktree, `esp_idf_size --diff`):

| Build | Image vs `60e7c647` | Sections |
|---|---|---|
| The DOS screen alone (`ccb09116`; `f5361508` changes tests only) | **+76 B** | `.text` +60, `.data` +16, DIRAM `.bss` 0 |
| Integrated (screen + shop transcript) | **+6,192 B** | `.text` +4,336, `.rodata` +1,840, `.data` +16, DIRAM `.bss` 0 |

The shop transcript's core code (`DosConsole`, the B-shops sequences) is shared core, so the firmware
compiles it, switched off (`ShopServices::dos_transcript` defaults to false and the T-Deck never sets
it). App image 1,095,328 B of the 1,310,720 B partition (16.4 % free); the launcher's 1,088 KiB
allocation is unchanged (18,785 B from the next 64 KiB step). No DIRAM growth beyond the 16 B of
`.data`. Not flashed. Logs: `native/targets/tdeck/origdos2-fw-*.log` (the integrated build). (A first
diff against ORIG-DOS1's own map also listed RTC sections that map lacked — a different build of the
same code; the base rebuilt here is the fair reference.)

**Host suite** (GCC, `native/core/build-dos`): 208 / 208, §7.

## 9. Screenshots (actual executable)

Taken with `UltimaVNative.exe` from the test package (§10), unpacked into a scratch folder, each run on
a fresh scratch data folder (the player's `%APPDATA%` data was not used or changed). They are game art,
so they are kept out of the repository, in `C:\Dev\uvw-original-dos-evidence\origdos2\` (its
`README.txt` indexes them):

- `screens\` — 84 pictures as the game draws them (what F12 saves; 320 × 200 on the DOS screen), the
  same sequences in three modes (`dos-` Original on the DOS screen, `hh-` Original on the handheld
  fallback, `enh-` Enhanced): the world, `Select:`, Ready, Use, Z-stats, Mix, the reagent seller, the
  blacksmith's `Arms`, the healer, the System Menu, a town and Talk, camp, combat, a dungeon, the
  blacksmith's Buy transcript, the tavern, the inn's rate line. 24 scripted runs, 0 failures.
- `windows\` — 36 windows as presented (read back from the renderer): Window 3× (1008 × 648 framed on
  the DOS screen, 1008 × 768 on the handheld one), Framed / Whole pixels / Fit at 1920 × 1080, Fit and
  Framed at 2560 × 1440, fullscreen on the offscreen driver.
- `real\` — **a real fullscreen window on this PC's 2560 × 1440 display** (Direct3D 11): Framed at 7×
  (`2560x1440 pillars 2240x1400@160,20`) and Fit (`2560x1440 fit 2304x1440@128,0`), both matching the
  layout pixel for pixel by the game's own frame checker, and the System Menu inside it.
- `sheets\` — the DOS 1988 captures beside Original (new), Original on the handheld screen and
  Enhanced (`sidebyside-screens-1..3`); the new screen across the game (`contact-dos-screen-*`);
  ORIG-DOS1's mockup B beside the real screen (`mockup-B-vs-actual`); Window 3× before and after.

## 10. Test package

- **File**: `native/targets/windows/dist/UltimaV-Native-Windows-x64-ORIGDOS2-caa20e72-test.zip`
  (this worktree), 2,324,398 B, SHA-256
  `1b12f197649ae8150397d005ee6003d79d7651acde719eaa5b0a15f824c55040`. A new name: the earlier test ZIPs
  are untouched.
- **Built** by `tools/build-windows.ps1 -Config Release -Test -Package -PackageName
  UltimaV-Native-Windows-x64-ORIGDOS2-caa20e72-test` from `caa20e72` with a clean tree; BUILD-INFO
  `Source: caa20e729e61… (windows/original-dos-interaction)`; the exe holds `caa20e729e61`; no game
  data. The packaged build passes **35 / 35** (`native/core/origdos2-win-package.log`).
- **Smoke**: the packaged exe ran the 24 screenshot scripts (§9) with 0 failures, and a real
  fullscreen run on a 2560 × 1440 Direct3D 11 display with pixel-exact layouts.
- **To see it as tested**: unzip into a new folder; the packs are the same as before (copy the three
  `openu5-*.bin` or run `Prepare-Game-Data.cmd`). Original opens on the DOS screen whatever an existing
  `windows.json` says about scaling; a saved `"scaling":"Integer"` (an earlier playtest's) shows it
  without the ring (Settings → Scaling → Framed adds it), and a saved window scale N gives the 200-row
  window at that N. `"originalScreen": "Handheld"` in that file brings back the 320 × 240 screen.
- **Not a release.** No person has played it.

## 11. Remaining (recorded)

- The title sequence, character creation, the frontend menus, the System Menu and Settings are the
  handheld's 320 × 240 screens (the 1988 game has its own 320 × 200 frontend; not in scope). They show
  at the largest whole scale inside the DOS-shaped window (2× in Window 3×).
- Rune lines (the Codex's prophecy, runic signs, `UiTextRune`) print in Latin on the console, as on the
  handheld; DOS prints them in RUNES.CH. The 13-column rune echo wrap is not emulated.
- `"originalScreen"` has no Settings row (the owner asked for the DOS screen as the default; the
  fallback is a `windows.json` key and a command-line option).
- The console's scroll-back shows no `►HISTORY◄` band (the web port's addition; DOS has no
  scroll-back).

## 12. Commits

On `windows/original-dos-interaction`, after `60e7c647`:

| Commit | What |
|---|---|
| `ccb09116` | the DOS 320 × 200 screen, the presenter and window, the console's 16 × 13, the startup lines, the profile hooks, the tests and scenarios |
| `f5361508` | the frame against ULTIMA.EXE's tables (D3a), the console wrap (D9b), the status box (D8b), the bullet's shape |
| `07edc45b` | the shop transcript (cherry-picked from `4de1d243`, `windows/original-dos-shops`) |
| `c6a8c6a8` | the handheld Original scenario carries the shop transcript's steps |
| `98e26f43` | the shop transcript's record (cherry-picked from `00e3bd22`) |
| `caa20e72` | this record, the README / fidelity / plan updates, the logs (the package is built from it) |
| next | this record's §9 / §10: the test package and the screenshots |

`windows/original-dos-shops` (`C:\Dev\uvw-dos-shops`) keeps its two original commits; it was not
merged.
