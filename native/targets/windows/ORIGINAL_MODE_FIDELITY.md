# Windows Original mode — 1988 interaction fidelity

Branch `windows/initial-port`. This document is the tracked fidelity matrix for
the Windows **Original** interface mode: for each interaction family it records
what the 1988 DOS game does (with the evidence), what Windows Original does now,
what Enhanced does, what changed in this batch, the tests that pin it, and what
remains uncertain or different. Decisions D22–D25 in
[`WINDOWS_PORT_PLAN.md`](../../../WINDOWS_PORT_PLAN.md) summarise it; §8 is
the audit showing that the T-Deck is unchanged.

## 1. Principle and architecture

- **Original** reproduces the DOS game's *interaction* semantics: single-key
  commands, the binary's prompt strings and rows, member choice on the roster,
  typed spell names, the kernel's key rules for every prompt. **Enhanced** keeps
  the handheld's menu-driven interface (list panels, key-legend bar, ESC
  cancels, RETURN passes, quick save/load, WASD).
- **One gameplay implementation.** Nothing in the core's rules, commands or RNG
  forks. The mode is one platform-profile predicate,
  `PlatformProfile::dos_interface` / `openu5::platform_dos_interface()`
  (`native/core/include/openu5/platform_profile.h`), installed by the Windows
  `DesktopProfile` as "interface mode is Original". Its null default is the
  T-Deck's interface, byte for byte: the 202 host tests of the baseline
  (including every panel golden) pass unchanged, the Board only gained view
  fields whose defaults draw today's pixels, and a differential run of the
  device runtime matches the baseline exactly (§8).
- Where the predicate is read: `UiSession` (key rules, prompt strings, the live
  prompt row, committed answers), `AlphaRuntime` (picker prompts and echoes, the
  composed views), `Board` (draws what the views say: reverse-video roster row,
  "Select:" caption, overlays without a footer). The Windows key map
  (`desktop_input.cpp`) differs between the modes only for the Camp guard's
  digits (below) and the Enhanced-only shortcuts it already had.
- Interface mode can be switched in Settings at any time; it changes how the
  player drives the game, never the journey's rules or saves (D8).

## 2. The 1988 primitives Original now uses

| Primitive | 1988 (evidence) | Windows Original |
|---|---|---|
| Command prompt | `\n` + the prompt glyph; the verb printed on that row (`Look-`, `Cast...`); an unknown key prints `What?`; RETURN and ESC are control codes → `What?` (MAINOUT 0x0bb8, TOWN 0x1544; kernel default 0x34d8) | bullet row, verb echo, `What?` for RETURN/ESC (no turn — see §5) |
| Roster picker `select_party_member` (ULTIMA.EXE 0x2d7a) | prints no text; "Select:" (DS 0x5554) centred in the panel's top band; the cursor row in reverse video (XOR, 0x2a28), starting on member 1 (0x2d82); digits 1–6 only move it (0x2dd4); arrows step and **clamp** (0x3f14/0x3f54); RETURN or Space choose (0x2e58/0x2e62); ESC = −1; `0` only for Z-stats; everything else re-read | no list panel; the roster row in reverse video; "Select:" in the status caption row (the T-Deck panel has no band row above the roster — §6); the same keys |
| `resolve_command_char` (0x4988) | combat → the actor; an active player → used silently; ≤1 member G/P → used silently / `None!`; else `Player: ` + picker; the name completes the row; a non-G/P pick → `Disabled!` + blank row + `Player: ` again; ESC → `None!` on the row | the shared seam `openu5/command_char.h` already modelled the branches; Original now prints `Player: Iolo`, `Player: None!`, `Player: Disabled!` + blank row on the console |
| Text entry `input_string` (0x3b1c) / `input_number` (0x3b9e) | the prompt then the text after it (most prompts end `\n:`); typed letters echoed in capitals (getkey 0x2032); Backspace or ← erase one; **ESC erases the line and keeps reading**; only RETURN ends | live prompt row on the console with the cursor; capitals; ESC erases; only RETURN ends |
| Y/N getkey | loops until Y or N; RETURN, ESC, others re-read (camp 0x3e3c, well LOOKOBJ 0x0052, quit CAST2 0x110b, guard TALK 0x00b9, trolls MAINOUT 0x1b80) | only Y/N; the answer completes the prompt row (`Save game? Yes`) |
| getdir (0x35EC) | only arrows and Space; Space prints `Pass`; **ESC is ignored** (0x3600–0x3620; the `cmp 0x1b` at 0x363b is dead); the word completes the verb row (`Look-North`) | no `Direction?` bar; ESC/RETURN ignored; Space → `Look-Pass` + the request's cancel path |
| Cursor | getkey_with_redraw 0x266c blinks the cursor at the text position at every getkey | the A4-UI4 wave cursor after every live prompt and getdir/aim row |

## 3. Fidelity matrix

Status key: **DOS** = matches the binary's interaction; **≈** = matches with a
noted presentation approximation; **Δ** = remaining divergence (recorded, not
changed in this batch); **?** = evidence insufficient.

### 3.1 Party / member selection

| | |
|---|---|
| 1988 | §2: `Player: ` (0x4988) for Cast, Search, Jimmy/Open (chest), Look at a crystal ball, dungeon Look; `Player: ` via ZSTATS 0x0000 (always asks, no shortcut) for Ready and Z-stats; raw 0x2e8e with the caller's own prompt for New Order (`\n\nSwap ` / `\nwith `), `On who: ` (CAST2 0x009e), the Camp guard (`Who will stand guard? `, name not echoed), the fountain (`Who will drink?`), the healer and the inn. |
| Windows Original (now) | all PartySelection pickers draw no panel: reverse-video roster row + "Select:" caption, console prompt + cursor, DOS keys, DOS echoes and cancel strings (`Player: None!`, `On who: None!`, `Swap nobody!`, `with nobody!`). Ready asks even with one member. |
| Enhanced | unchanged: the "Choose Companion" / "Ready Whom?" / "Use On Whom?" / "Spell Target" / "Move From/To" list panels with their bar; digits highlight and Space chooses (D16); the Camp guard's digit picks at once. |
| Changes | `compose_selection_view` returns no panel for PartySelection; `DevicePartyHighlight.cursor/select_banner`; `open_selection` prompts and initial cursor 0; `modal()`/`accept_command_char_pick()` echoes; UiSession clamped arrows, Space, digit highlight (Camp guard included), Z-stats `0`. |
| Tests | `scenario_typed_cast_original` (cursor start, digit, clamp, Space, ESC), `scenario_original_interface` (Ready, Z-stats, New Order), `scenario_original_camp` (guard), `scenario_enhanced_interface` (panels kept), `uv_desktop_input_test` P1–P6, host tests (§4). |
| Status | **DOS** for keys, prompts and echoes; **≈** "Select:" placement (§6). |
| Uncertainty | none for the picker itself. The active player is not re-checked for status by 0x4988 (as native). |

### 3.2 Casting (outside combat)

| | |
|---|---|
| 1988 | `Cast...` → caster (0x4988) → `Spell name:\n:` (DS 0x4603) → CAST2 0x00DE typed syllables echoed on the `:` row (13-column wrap, J/O ignored, ≤4, Backspace takes one back, RETURN/Space end, ESC = nothing typed but the echo stays) → `None!` on that row / `No effect!` on its own row → location gate → `None mixed!` → MP → level → handler (target prompts last). A repeated initial counts once (0x02b2–0x02cb). |
| Windows Original (now) | exactly that row layout: `Cast...` / `Player: Iolo` / `Spell name:` / `:IN LOR` (live while typing), `:IN LOR None!` after ESC, `No effect!`; `IIL` casts In Lor. Gate order, MP and target prompts come from the core as before. |
| Enhanced | the T-Deck spell list (A-17), unchanged. |
| Changes | `begin_spell_name()`, the SpellName arm's DOS rows, ESC keeping the syllables (`UiModalValue::number = kSpellEntryNone`), `spell_from_initials` duplicate collapse. |
| Tests | `scenario_typed_cast_original`, host `win_dos_interface` (spell matcher). |
| Status | **DOS**; **≈** the 13-column echo wrap (the console wraps at its own width). |
| Uncertainty | turn cost of the early exits after `None!`/`No effect!`/`None mixed!` (residual AX, §5). |

### 3.3 Casting in combat

| | |
|---|---|
| 1988 | `Cast...` before the absorb gate (COMBAT 0x08f0/0x092f), the actor casts (no `Player:`), `Spell name:` typed, every outcome ends the turn; heal spells `On who: ` on the roster; weapon spells `Aim! ` + crosshair; charm spells `Creature: ` + crosshair; line spells (In Zu, In Nox Hur, In Flam Hur, In Vas Grav Corp) a getdir (CAST 0x1f8a). |
| Windows Original (now) | as before plus the DOS rows; `On who: ` on the roster; `Aim! ` echoed before the crosshair; no bar. |
| Enhanced | spell list, reticle with the `Aim:` readout bar. |
| Status | **DOS** for the cast and `On who:`; **Δ** line spells use the reticle instead of a getdir and charm spells print `Aim! ` instead of `Creature: ` (the core's target model decides the prompt; changing it is a core change shared with the T-Deck). |

### 3.4 Spell targeting

| | |
|---|---|
| 1988 | `On who: ` (always asks, ESC → `None!`, spell already spent); `Direction-` + getdir (CAST2 0x0306; Space = Pass); `To phase: ` one key echoed (any non-1–8 key fails). |
| Windows Original (now) | `On who: Shamino` / `On who: None!`; `Direction-` row with the word appended, Space = `Direction-Pass`; `To phase: 3`, ESC fails the spell. |
| Status | **DOS** for prompts/keys. **Δ** native refunds the spell when `On who:` is cancelled (D-101, a core rule shared with the T-Deck). **SPP1 F (2026-10-14):** the refund is gone on every platform. The charge and MP stay spent, the gates come before the prompt, and the `Player:` caster pays. The ceremony for a cancelled world heal remains (D-101). |

### 3.5 Mixing

| | |
|---|---|
| 1988 | `Mix Reagents` + blank row; `No reagents owned!`; `For what spell?\n:` typed; −1 → `\nNone!`; **−2 (no such spell) goes on** to the reagent picker and any mix is the wrong mix (CMDS 0x1b13 / 0x1bc2); the legend `←,→,↑,↓ to move,` / `RETURN selects.` / `Type M to mix:` in the console; the `Reagents:` panel (legitimate right-panel list); `How much? ` getnum (ESC erases); `Insufficient reagents!` re-asks. |
| Windows Original (now) | the same, with the legend printed as `Arrows to move,` (the console face has no arrow glyphs); the unmatched name continues as spell 48 into the core's wrong-mix path; the `Reagents:` panel without its footer. |
| Enhanced | the spell list and the A-17 footer legend, unchanged. |
| Status | **DOS**; **≈** the arrow glyphs spelled `Arrows`. |
| Uncertainty | where the console breaks between `How much? N` and the next line (print_string word-wrap, not traced). |

### 3.6 Use

| | |
|---|---|
| 1988 | `Use item` + blank row; `No usable items!` (no list); `Item: ` with the `Items:` panel (legitimate list); RETURN/Space choose; ESC → `Item: None!`; the handler prints its own name after `Item: ` (`Potion`, `Scroll`...); potions `On who: `; scrolls/skull key `Direction-`. |
| Windows Original (now) | `No usable items!`; `Items:` panel without detail or footer; `Item: ` in the console strip; Space/RETURN; `Item: None!`; `On who: `; `Direction-`. |
| Status | **DOS** for the list, keys and rows: `Item: Potion` / `Item: Scroll` (+ blank row) are printed at the pick, before `On who:` / `Direction-` (`Command::item_named`, default false, keeps the core from repeating the name); `On who: None!` still uses the potion up (CAST 0x136a decrements it before the prompt; native's D-51 refund stays on the T-Deck and in Enhanced). **≈** in the arena and for the other items (carpet, skull key, moonstones, sextant...) the core still prints the name on the row after `Item:`; **?** whether a cancelled resurrection scroll is spent (the decrement precedes the handler, not traced per scroll) — native keeps it. |

### 3.7 Ready / equipment

| | |
|---|---|
| 1988 | `Ready...` + blank row; `Player: ` always (ZSTATS 0x0000); `Thou art empty-\nhanded!`; `Item: ` + the member's own list (banner = the name; legitimate list); ready/unready print **nothing**; a refusal prints `\n\n<message>\n\nItem: `; ESC → `Done`; arena: the actor, the turn ends. |
| Windows Original (now) | as 1988; the native `Equipped X.` / `Unready X.` lines are gone in Original; a vanishing ring prints `Ring vanishes!` and closes the list (ZSTATS 0x0e45, the only success that does), ending R in the arena. |
| Enhanced | `Ready Whom?` / `Ready Equipment` panels with slot detail and footer; the feedback lines. |
| Tests | `scenario_original_interface` (Player:, list title, no feedback, `Item: Done`), `scenario_enhanced_interface`. |
| Status | **DOS**; **Δ** turn cost outside combat (§5). |

### 3.8 Z-stats and inventory inspection

| | |
|---|---|
| 1988 | `Z-stats...`; `Player: ` always; `0` → the Equipment page; `\nStatus: ` waits while the pages show; Space/ESC close → `Done`; arena: the actor's page with no prompt, the turn ends. The pages are the only inventory inspection (legitimate right-panel pages). |
| Windows Original (now) | as 1988: roster picker, `Status: ` ... `Done`, the pages without the member counter and footer, the console strip below them; arena `Z` shows the actor's page and ends the turn on close (it was `What?`). |
| Enhanced | `Z / Stats` preview panel, pages with footer; arena `Z` = `What?` (unchanged). |
| Status | **DOS** (keys/prompts); **≈** pages keep the native unabbreviated names (A-11/A-12); **Δ** turn cost outside combat (§5). |

### 3.9 Direction commands, Get (there is no Drop)

| | |
|---|---|
| 1988 | Look/Get/Open/Search/Jimmy/Push/Klimb/Attack/Talk use getdir; `Get-Pass` etc.; `D` → `D-What?` — **Ultima V has no Drop command**; Board/X-it/Enter/Ignite ask no direction; the dungeon's search is `Search...` then a `Dir-` prompt. |
| Windows Original (now) | no bar; ESC/RETURN ignored; Space → `<Verb>-Pass`; Talk's word on its row (`Talk-North`); the frigate's Fire is one key (`Fire-North`); dungeon `Klimb-U/D-` takes U/D and the answer stays on that row; dungeon `Search...` / `Dir-Ahead`; arena getdir Pass ends the actor's turn. |
| Enhanced | `Direction?` bar, ESC cancels silently. |
| Status | **DOS** keys/echo; **Δ** world getdir Pass costs no turn (§5); **Δ** dungeon Get/Open/Jimmy/Search details (core). |

### 3.10 Talk

| | |
|---|---|
| 1988 | `Talk-` + getdir; `Your interest?\n:` and `You respond-\n:` typed in capitals (cap 15); empty → BYE; **no cancel key** (ESC erases the word); `Nobody's here!` in a town. |
| Windows Original (now) | `Your interest?` / `:JOB` on the console, capitals, ESC erases, conversations have no cancel key. |
| Enhanced | the bar `Your interest? >job`; ESC/Back end the conversation. |
| Status | **DOS**; **Δ** the core answers an empty town cell with `Funny, no response!` (the overworld string). **SPP1:** the binary's text is TALK 0x0491's `Nobody's here!` (DS 0x91d6); still open as ledger D-111. Its turn is now charged (TALK → 0 → 1). A conversation keeps its old cost (none; D-107). |

### 3.11 Shops and services

| | |
|---|---|
| 1988 | everything in the console: lettered stock lists, pitches, `Y/N` questions that re-read everything else; the buyer is the active player or the first conscious member; the healer's `"Who needs my aid?"` and the inn's `"Who will stay?"` are the roster picker; the only panel lists are the blacksmith's Sell `Arms` window and the inn's `GUEST REGISTER`. |
| Windows Original (now) | the shop panel (offers, gold, log) without its key-legend footer; every deal question takes only Y/N; the greeting and "anything else?" gates take Y, N and Space as N (Y/N only at the healer and the drink warning); the blacksmith's door takes any key; the service menus ignore ESC (RETURN is "Nothing" at the healer); the healer's and the inn's member choice is the **roster picker** (reverse-video row, "Select:", digits highlight, RETURN/Space) instead of the panel; the guest register RETURN/Space; the rumor `You respond:` and rations `How many wouldst thou like?` prompts. |
| Enhanced | unchanged. |
| Status | **≈/Δ**: keys and questions are DOS; **Δ** the offers are a right-panel list (the T-Deck's shop panel) rather than console lines — a console-only shop presentation would be a new presenter for the whole shop flow (§7). |

### 3.12 Inn / rest / camp

| | |
|---|---|
| 1988 | camp: `Hole up & camp!` + blank row, `For how many hours? (1-9) ` one getkey (`0`/Space abandon, echoed), `\nWilt thou set a watch? ` Y/N, `Who will stand guard? ` roster, `None posted!`; town bed `For how many hours? `; off-bed `Hole up- Only in bed!` with no question; inn as shops. |
| Windows Original (now) | all of the camp; the bed's `Hole up-` / `For how many hours? `; aboard ship `Hole up &` / `repair...` and the core's repair (or "Sails must be lowered!"); off a bed in a town the core's `Hole up- Only in bed!` with no question; on water or mounted `Hole up & camp!` + the core's refusal. |
| Tests | `scenario_original_camp`. |
| Status | **DOS**; **≈** the bed's `Hole up-` and `For how many hours?` are two rows (DOS prints them on one). |

### 3.13 Quest prompts

| | |
|---|---|
| 1988 | shrine: `Upon what virtue dost thou meditate?\n\n:` then `\nMantra:` three times (input_string 12); no "Visit?" question; donation `Offer how many hundredweights gold? ` one digit; guard password (event text + `\n\nYour response?\n`), tribute `Dost thou pay?` (`No!`), arrest, troll `Dost thou pay?` (key echoed), well `Drop a coin?` Y/N only then `Thy wish?`, fountain `Who will drink?`, Blackthorn `Your response?\n:`, `\nDost thou wish to leave? `. |
| Windows Original (now) | all of those strings and key rules: the shrine asks three mantras with the binary's rows (the Yell restore uses DS 0x4450's own line breaks); the arrest's `"Wilt thou come quietly?"` + `:` with the core's single `Yes`/`No` answer line; the troll's `Caught!` / `The trolls demand a N gp toll!` / `Dost thou pay?` and the guard's demand with its sum (or Minoc's charity) before `Dost thou pay?`; the town exit takes Y, N or ESC (= No, TOWN 0x07a2-0x07b9 — the one Y/N that leaves on ESC); `:Nothing` for an empty Yell and `:BYE` for an empty interest on the prompt row. |
| Status | **DOS**; **≈** the donation's `500 gp` is printed by the core on its own row. |

### 3.14 Combat commands

| | |
|---|---|
| 1988 | arrows move; Space `Pass`; `A` → `Attack-Aim! ` crosshair (RETURN/`A`/Space aim, ESC loses the swing: `Nothing!`, turn spent); ESC = Escape (`-Not yet!` / `-Not here!` / leave after victory — CMDS 0x17ec); RETURN/Backspace `What?`; B/E/F/H/I/L/M/N/Q/V/X/T/D/W print COMBAT.OVL's refusals; J/O/P/S/G/Y/Z/K/R/U/C are commands; digits set the active player. |
| Windows Original (now) | *(SPP1 C: J/P/Y now work too; see Status.)* all of those except J/P/Y (still `What?`, §7); every arena command ends the actor's turn as COMBAT.OVL does, cancels included (a cancelled spell aim, ESC at the Use list or at a heal spell's `On who:`, an empty Use or Ready); RETURN or `A` with the crosshair on the actor's own cell is ignored (melee and spells). ESC = Escape was already the core's DOS behaviour. |
| Enhanced | RETURN = Pass, Backspace = flee, aim ESC free, `Attack` echo, `What?` for the refused letters. |
| Tests | `scenario_original_arena`, `scenario_typed_cast_combat`, `scenario_surface_combat`. |
| Status | **DOS** except **Δ** J/P/Y in the arena (core commands missing). **SPP1 C (2026-10-14):** J/P/Y are COMBAT.OVL's, in the shared core, on every platform. Original: `Jimmy-` / `Push-` with Space = Pass, and `Yell what?`. The scenario is `spp1_original_arena`. |

### 3.15 Save / quit

| | |
|---|---|
| 1988 | `Quit:` then `\nSave game? ` (Y/N only), `Yes` `Saving...` `Done.`; play continues; no turn; Ctrl-E `Exit to DOS? `. |
| Windows Original (now) | as D15, with `Save game? Yes` / `Save game? No` on the prompt row and no bar. The System Menu (F10) stays for slots/settings. |
| Status | **DOS**; Ctrl-E is the window's close button (Windows asks before closing). |

## 4. Tests

| Test | Kind | What it pins |
|---|---|---|
| `scenario_typed_cast_original` | Windows, real exe | roster picker for the caster, `Spell name:` rows, IIL, ESC, No effect!, Player: None! |
| `scenario_original_interface` | Windows, real exe | Ready, Z-stats, Use, New Order, getdir, Talk, Yell, What?, Mix, Quit |
| `scenario_original_arena` | Windows, real exe (Developer) | arena refusals, Attack-Aim!/Nothing!, Z in combat, getdir Pass, turns |
| `scenario_original_camp` | Windows, real exe (Developer) | camp hours/watch/guard |
| `scenario_enhanced_interface` | Windows, real exe | Enhanced keeps panels, bars, ESC cancels, RETURN passes |
| `scenario_original_keys` | Windows, real exe | Enhanced shortcuts refused, Q prompt |
| `uv_desktop_input_test` | Windows unit | the key map in both modes |
| host `win_dos_interface` / `win_dos_runtime` | GCC host suite | the shared UiSession rules and the real Board's pixels with the DOS profile, and the T-Deck control |

## 5. Turn and time — recorded, not changed

The kernel dispatcher (ULTIMA.EXE 0x3178) returns 1 — a world turn — for
almost every key, including Ready, Z-stats, New Order and Use (0x339a, 0x3472,
0x334e, 0x340c), every getdir `Pass`, and most refusals; only an unknown key,
D, W, Q and a few overlay results return 0. Native's core charges no turn for
Ready, Z-stats, New Order, Use or a world getdir cancel. These are **turn rules
of the shared core**, also on the T-Deck, and several project notes state the
opposite (they relied on `g_unk_24e6`, which is a repaint flag); changing them
belongs in a shared parity batch with RED-first core tests, not in a Windows
interface mode. In the **arena**, where native's free cancels were a declared
handheld UI contract, Original now ends the actor's turn exactly as COMBAT.OVL
does, through the core's existing actions (`CombatAttackCancel`,
`CombatYield`).

**SPP1 update (2026-10-14).** The shared parity batch this section asked for
is SPP1 A ([`docs/SHARED_PRESERVATION_PARITY_SPP1.md`](../../../docs/SHARED_PRESERVATION_PARITY_SPP1.md) §3.3). Native now charges the dispatcher's turn for R, Z, N, U, C, M and
the getdir commands on every path. That covers cancels, refusals and a getdir Pass, on
the T-Deck and in both Windows modes, through one latch in
`AlphaRuntime::route_input` and the core's `CommandKind::DispatcherTurn`.

Still open (ledger D-107):
- the town conversation (TALK's nonzero return), which keeps its old cost (none);
- H;
- the B / E / Y overlay returns;
- town and dungeon K.

## 6. Presentation approximations

(Recorded adaptations of the 1988 screen to the 320 × 240 layout the Windows
build shares with the T-Deck.)

- **"Select:"** appears in the status caption row (y 58): the T-Deck panel has
  only a 2-px band above the roster, so the DOS 8-px banner row has no place
  without moving the boxes (shared layout).
- The console is the T-Deck's 22-column window, not DOS's 16; long prompts wrap
  where the window ends (the rune echo's own 13-column wrap is not emulated).
- The roster's green/cyan tints are off in Original; the active player keeps
  its arrow; the arena actor's row is in reverse video.
- List panels (Ready, Use, Mix reagents, Z-stats, shops) keep the T-Deck's
  panel drawing (title band, rows, the console strip below) without slot
  details or key legends.
- The Mix legend's `←,→,↑,↓` is printed `Arrows` (the console face has no
  arrow glyphs).
- A console scrolled back with PgUp hides the live prompt row until it
  follows the newest line again.

## 7. Remaining Original-mode divergences (recorded)

1. World turn costs (§5) — shared core. **SPP1 A: fixed** (D-107 holds the remainder).
2. *(Still open after SPP1: ledger D-105 P1. SPP1 B fixed only the paid
   rumour's missing text, P2, on every platform.)* Shop offers as a right-panel list instead of console lines (the buy
   lists, pitches and prices are a new presenter for the whole shop flow).
3. *(SPP1 C: J/P/Y fixed. SPP1 D: in Original, the line spells read `Direction-`
   and charm / clone print `Creature: `. The per-turn actor banner is still
   missing.)* Arena J/P/Y missing; line spells use a reticle instead of a getdir; charm
   spells print `Aim!` instead of `Creature:`; no per-turn actor banner line —
   core.
4. *(SPP1 E: the dungeon Look is ported. Original's `D` is `D-What?`; the
   T-Deck / Enhanced `D` is the Look aimed Here, ledger E-3. The dungeon Get /
   Open / Jimmy / Search member pick is still missing.)* Dungeon Look (DNGLOOK) missing, so the dungeon `D` (drink) shortcut stays;
   dungeon Get/Open/Jimmy/Search take no member pick (0x4988) — core.
5. `Funny, no response!` for an empty town cell (core string). *(SPP1: the
   binary prints `Nobody's here!`, TALK 0x0491 / DS 0x91d6; ledger D-111. The turn is now charged.)*
6. *(SPP1 F: fixed. A cast the gates refuse opens no target prompt.)* Spell target prompts (`Direction-`, `Aim!`, `To phase:`) open before the
   location/MP/level gates refuse a cast (the gates live inside the core's
   cast; a pre-existing order shared with the T-Deck, plan §8).
7. Native refunds a heal spell when `On who:` is cancelled (D-101) — core. **SPP1 F: fixed** (the ceremony residual stays in D-101).
8. Ctrl-K (karma), Ctrl-V (version), Ctrl-B (buffer) not implemented. **SPP1 G:** Ctrl-K and Ctrl-V work. Ctrl-B is deliberately unmapped (a BIOS type-ahead switch; ledger D-108).
9. Arena Use and the non-potion items print their name on the row after
   `Item:`; the arena's `None!` after a typed spell is its own row.
10. 22-column console, the 13-column rune wrap, "Select:" placement, the
    Mix legend's arrows spelled `Arrows` (§6).
11. Rel Hur asks no `Direction-` (the core's target model) — core. *(Still open after SPP1: ledger D-106, reference first.)*

## 8. T-Deck preservation audit

The requirement: the T-Deck firmware and everything its player sees must be
unchanged by this work. The audit compares against the **baseline branch
commit `2c8de821`** (the Windows port before this batch), checked out in a
separate scratch worktree. A successful firmware compile alone was not
accepted as evidence. No device was flashed and the installed firmware was
not touched.

**Shared files changed.**
- Core: `platform_profile.{h,cpp}`, `ui_session.{h,cpp}`,
  `spell_entry.{h,cpp}`, `commands.h`, `world_magic.cpp`.
- Device application: `alpha_runtime.{h,cpp}`, `device_ui_views.h`,
  `tdeck_board.cpp`.
- Everything else is under `native/targets/windows` or is a host test.

### 8.1 Code audit (every shared hunk)

| Change | Why the T-Deck cannot see it |
|---|---|
| `PlatformProfile::dos_interface` + `platform_dos_interface()` | the device never calls `set_platform_profile` (`main.cpp`), so the hook is null and the accessor returns false |
| UiSession key rules, prompts, the live prompt row, `dos_commit_answer`, Hole up, getdir Pass | every branch tests `platform_dos_interface()` first; the live prompt row is streamed only when it is on |
| AlphaRuntime prompts, echoes, pickers, Z/Ready/Use/Mix/shrine/quit wording, composed views | gated on `platform_dos_interface()` (or the pre-existing `typed_spell_names` / `quit_save_key`, also null on the device) |
| `spell_from_initials` collapses duplicate initials | its only callers handle the SpellName / MixSpellName requests, which open only under `typed_spell_names()` |
| `Command::item_named` | defaults to false, so `world_magic` prints `Potion` / `Scroll` exactly as before |
| `DevicePartyHighlight::{cursor, invert_actor, select_banner}` | the defaults (−1, −1, false) take the old branches in `draw_party_rows` |
| shop/selection overlays draw their context bar only `if (context.active)` | outside Original mode, `compose_shop_view` and `compose_selection_view` set `context.active` on every path that returns a view (as at the baseline); only the 1988 model clears it; the panel goldens and the differential run confirm the pixels |

The audit found three issues, all fixed in `81fb61d2`:
- The arena `OpenStatusSelection` (Z) branch in AlphaRuntime was not gated. On the device it still printed `What?` (the probe confirms it), but it was not gated by construction.
- The 1988 model's state (the mantras, the Z-stats turn flag, the ring flag) added 112 B to internal `.bss`. It now lives in a `DosInterfaceState` that is allocated only when Original mode first needs it.
- The console line streamer grew by about 256 B of stack: a feed lambda plus a stack `UiTextBlock`. It is one loop again, and the live block now lives in UiSession storage, which is PSRAM on the device.

### 8.2 Firmware memory (ESP-IDF 6.1, same sdkconfig, `esp_idf_size --diff`)

| Section | Baseline `2c8de821` | HEAD | Δ |
|---|---|---|---|
| Flash `.text` | — | — | +9,736 B |
| Flash `.rodata` | — | — | +1,472 B |
| DIRAM `.bss` | — | — | **+16 B** (the `unique_ptr` and the live block pointer path; was +112 B before 81fb61d2) |
| DIRAM `.data`, IRAM | — | — | 0 |
| Image | 1,053,808 B | 1,065,008 B | +11,200 B |

- The Launcher allocation is unchanged at 1088 KiB (17 × 64 KiB in both).
- The 1.25 MiB factory partition keeps 18.7 % free.
- Both images were already larger than the 1 MiB `ota_3` slot used for the esptool test path, so that limit was already exceeded at the baseline; the Launcher install is the route.

### 8.3 Stack (`-fstack-usage`, firmware build)

| Function | Baseline | HEAD |
|---|---|---|
| `UiSession::handle_modal` | 208 | 288 |
| `for_each_wrapped_line` | 544 | 560 |
| `AlphaRuntime::handle_zstats_input` | 848 | 880 |
| `AlphaRuntime::open_selection` | 368 | 400 |
| `AlphaRuntime::modal` | 224 | 256 |
| `AlphaRuntime::accept_command_char_pick` | 32 | 64 |
| `compose_party_highlight` / `compose_shop_view` / `refresh_session_context` | 64 / 64 / 80 | 80 / 80 / 96 |
| `Board::draw_party_rows` | 96 | 112 |

- No frame grew by more than 80 B.
- The new 1988-only functions (`dos_commit_answer` 208, `dos_getdir_pass` 144, `dos_hole_up` 128, the shop Y/N lambda 128) are never called on the device.

### 8.4 Host suite

- The baseline's 202 tests all pass unchanged, including every A3-04F / A4-UI1 panel golden and the A4-ENH1 / ENH2 preservation tests.
- The two new tests (`win_dos_interface`, `win_dos_runtime`) bring the total to 204/204.
- `win_dos_runtime`'s T group runs the T-Deck control in the same binary.

### 8.5 Differential behaviour run

A throwaway host program, not committed, was built twice from the same source: once in the baseline tree and once in HEAD. It used the A4-UI2 harness: the real AlphaRuntime, the real Board over the fake ST7789, the virtual clock, the real resource and audio packs, the memory save card and the default profile.

It played identical raw T-Deck inputs: letters, Return, Backspace, Mic, trackball moves and click, and Alt+M/S/L. After **each of 7,653 inputs** it hashed:
- the whole 320 × 240 GRAM;
- the transcript (text, channel and flags of every block);
- `GameState`;
- the arena (current actor; each actor's position, HP and status);
- the audio backend's calls (with timestamps);
- the last save's GAM bytes;
- the UI mode, request and prompt;
- the console cursor.

The walks:
- every command letter A–Z × 8 response scripts on the surface, with and without an active player;
- 33 arena scripts (attack, cast, Ready, Use, Z, refusals);
- all eight shop types × 8 scripts;
- ordinary conversations;
- every letter × 4 scripts in a town, in Blackthorn's palace and in two dungeons;
- save, load and System Menu rows, Settings, Quit, camp, New Order, Z, Yell, View, Push;
- the shrines: Visit, virtue, mantra, cycles.

**Result:**
- The baseline and HEAD outputs are byte-identical: 57,304 lines, including the runtime's own log lines, SHA-256 `26d0979a…` for both.
- **Positive control:** the same HEAD binary with the Windows DOS profile installed differs on almost every step (for example, Cast shows `Player: Avatar` / `Spell name:` / `:` in place of the spell list), so the probe does see the Original-mode changes.

### 8.6 Not covered

- No hardware run: no device was flashed, by instruction. The evidence comes from host builds and the firmware build only.
- Scenes the walks did not reach (the endgame, Blackthorn's capture, the trolls' toll, wells) are covered only by the code audit and the existing host tests.
