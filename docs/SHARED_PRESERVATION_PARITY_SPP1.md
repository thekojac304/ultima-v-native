# Shared Preservation Parity, Batch 1 (SPP1)

**Branch** `parity/shared-preservation-1` · **worktree** `C:\Dev\ultima-v-native-parity` · **base**
`e089f2c5` (`windows/initial-port`, unchanged) · **dates** 2026-10-13 – 2026-10-14 · **not pushed,
merged, published or flashed.**

SPP1 corrects preservation defects that the Windows Original-mode fidelity audit found
(`native/targets/windows/ORIGINAL_MODE_FIDELITY.md` §5, §7). One, B's paid rumour (P2), was found by
SPP1's own shop research. They are corrected where they live, in the shared core (`native/core`) and
the shared application layer (`native/targets/tdeck/main`). The T-Deck and both Windows modes
therefore get the same gameplay. No gameplay rule is Windows-only. Only two things are Windows-only:
- the desktop key map, Ctrl+K / Ctrl+V in `desktop_input.cpp`;
- Original's 1988 presentation (`D-What?`, `Direction-`, `Creature: `, `Jimmy-Pass`), behind
  `platform_dos_interface()`, which only Windows Original installs.

Status words used below:

| Word | Meaning |
|---|---|
| **CONFIRMED** | Proven from the DOS binaries by static disassembly, at the cited offsets. No DOS session was recorded: no DOSBox run exists for any item. |
| **IMPLEMENTED + AUTO-VERIFIED** | Implemented in shared code. Proven by host tests that were RED before the fix and GREEN after, run on the real AlphaRuntime and Board in both profiles; by mutation tests; by the Windows suite (Windows scenarios exist for G, C and E only); and by the T-Deck differential. |
| **NOT HARDWARE-TESTED** | Every item. Nothing in SPP1 was run on a T-Deck: none was available, and nothing was flashed. The Windows checks are automated runs of the real executable (headless scenarios), not manual play. |
| **DEFERRED** | Identified, and in most cases CONFIRMED, but not changed. Usually a reference-pinned corpus needs the TypeScript reference fixed first ("fix the reference at the pinned layer"). Otherwise it is unresolved evidence, or out of scope. |

## 1. Baseline

The baseline is `e089f2c5` in the worktree's LF checkout. A CRLF first attempt failed 20 drift tests; that is a
checkout artefact, recorded in `native/core/spp1-baseline-ctest-crlf-attempt.log`.

| Check | Result | Log |
|---|---|---|
| Host suite (GCC 16, serial ctest) | **204/204** | `native/core/spp1-baseline-ctest.log` |
| Windows suite (MSVC, 8 unit + 18 scenarios) | **26/26** | `native/core/spp1-baseline-win.log` |
| T-Deck firmware (ESP-IDF 6.1, baseline worktree `C:\Dev\uvw-spp1-base`) | `0x104020` (1,064,992 B); 245,728 B (18.7 %) of the 1.25 MiB app partition free | §7.3 |

**DOS evidence sources.** The research notes are in `native/core/spp1-findings/` (commit `b7593ced`):

- `A-turn-costs.md`
- `B-shops.md`
- `C-arena-jpy.md`
- `DF-spell-targeting-heal-cancel.md`
- `E-dungeon-look.md`

They disassemble the install in `original/u5/ultima5` with `re/tools/dis16.py`, at the fitted overlay
bases. The resident kernel and the context loops are 2001-dated *Ultima V Upgrade 1.0* files. Whether a
pristine 1988 `ULTIMA.EXE` has the same dispatcher return convention is **UNKNOWN** (A §9.1). Every
claim below is about this binary set. Offsets are re-checked in the commits that use them.

## 2. Summary

| Item | Discrepancy (before SPP1, every platform) | DOS evidence | Layer | Status | Commit |
|---|---|---|---|---|---|
| **G** | Ctrl-K / Ctrl-V answered `What?` | MAINOUT table 0x0bbe, TOWN 0x1552, DUNGEON 0x06c4 → karma (kernel 0x1a3e) / `1.16` (DS 0x2b4f) | core + UiSession + Windows key map | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED | `0b1fd6c0` |
| **F** | A cancelled heal target refunded the charge and the MP; gates ran after the target prompt; the `Player:` caster did not pay; a cancelled potion / In Mani Corp target refunded the item (D-51) | CAST 0x0dba gate order 0x0e1a → 0x0f01, arm 0x0f1a; CAST2 0x009e; CAST 0x136a / 0x11ec | core (magic, combat) + runtime | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; ceremony residual DEFERRED (D-101) | `c0e95816` |
| **A** | R, Z, N, U, C, M and the getdir commands (and their cancels, refusals and Pass) cost no turn | K:0x3178 returns 1 on every path; MAINOUT 0x0c39, TOWN 0x15d0, DUNGEON 0x0f84 | core (`DispatcherTurn`) + runtime latch | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; conversations and H/B/E/Y DEFERRED (D-107) | `5b9e877b`, `7728250d`, `5ba2fa46` |
| **C** | The arena's J, P and Y answered `What?` | COMBAT 0x097a / 0x0994 / 0x09c0 → SJOG 0x0d4a, CMDS 0x161a, CMDS 0x1418 | core (combat) + UiSession | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; residuals DEFERRED (D-110) | `f8ad45cb` |
| **E** | No dungeon Look (`What?`); the T-Deck `D` drank for the active member and invented `No fountain here.` | DNGLOOK 0x0000 (via K:0x3310); `D` is K:0x324e `D-What?` | core (dungeon) + UiSession + runtime | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; E-3 amended; TS reference DEFERRED (D-109) | `d049500c` |
| **D** | Arena reticle for 3 no-prompt spells; no town An Ylem; Original lacked `Direction-` / `Creature:` | CAST 0x043e / 0x05f3 / 0x074c; CAST 0x0230; CAST 0x1f8a, DS 0x45bb / 0x45dc | core (magic, world magic) + runtime | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; corpus-pinned spells DEFERRED (D-106) | `dc6c38bd` |
| **B** | A paid tavern rumour printed nothing | SHOPPES2 0x0636-0x065c; DS 0x9F82 / 0x9F8A | core (shop orchestration) | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED (P2); S1-S19, P1, P3-P7 DEFERRED (D-104, D-105) | `c4f53082` |

The corrections went in this order: G → F → A → C → E → D → B. A's conversation fix came from the
T-Deck differential (§7.2).

## 3. The corrections

### 3.1 G — Ctrl-K and Ctrl-V

- **DOS.** The context loops' control-key tables send 0x0B to the karma print (kernel 0x1a3e, pad
  `' '`, width 1: a bare decimal) and 0x16 to `1.16` (DS 0x2b4f). The echo is on the command row,
  followed by a newline.
  - On the surface (MAINOUT 0x0af8) and in a town (TOWN 0x154b) there is no turn.
  - In a dungeon the handler keeps its default return of 1 (0x06cb), so the dungeon turn block 0x0c76
    runs once.
  - The arena has neither key (COMBAT 0x0881-0x0886: `What?`).
- **Ctrl-B.** It is the dispatcher's "Buffer On/Off": getkey 0x2712 turns code 2 into 0xFC, and
  kernel 0x31c6 toggles the BIOS type-ahead flush 0x1b24. That is a keyboard-hardware switch with no
  counterpart here, so it is **left unmapped** (D-108).
- **Layer.** Core `CommandKind::ControlKey` (appended): a `WalkEcho` line, and underground the existing
  `DungeonAction::Tick`. UiSession routes 0x0B / 0x16 from the exploration and dungeon prompts. Windows
  `desktop_input.cpp` maps Ctrl+K and Ctrl+V to those codes in both modes.
- **Before → after.** Every platform printed `What?`. Now the karma (`60`) and `1.16` print; underground
  one dungeon turn passes.
- **T-Deck.** The keyboard matrix has no Ctrl key, so the device cannot type either code (G5). The
  shared path is exercised by the differential's raw codes.
- **Tests.**
  - `spp1_runtime` G1-G5: 7/13 RED before the fix.
  - `uv_desktop_input_test` K1-K3: 4 RED before the fix.
  - Scenarios `spp1_original` (Ctrl+V, and Ctrl+K printing this creation's karma, `75`) and
    `spp1_enhanced` (Ctrl+V).
  - Mutants G1-G5 killed.

### 3.2 F — the target pick and the gates of Cast and Use

- **DOS.** CAST.OVL 0x0dba is the only Cast routine (world, town, dungeon, arena). It runs in this
  order, and only then jumps to the spell's arm (0x0f1a), where every target prompt lives:
  1. location gate (0x0e1a);
  2. None mixed (0x0eb8);
  3. spend the charge (0x0ec8);
  4. check the MP (0x0ecc);
  5. spend the MP (0x0ef8);
  6. check the level (0x0f01).

  ESC at `On who: ` (CAST2 0x009e) makes the arm return −1: no `Success!` / `Failed!`, and the charge
  and MP stay spent. In the arena COMBAT 0x0923 has already ended the actor's turn. (U)se spends a
  potion at CAST 0x136a and the In Mani Corp scroll at 0x11ec, both before `On who: `.
- **Layer.**
  - Core: `cast_gate()` is the gates as a pure function, and `cast_spell()` uses it.
    `spell_targets_member()` is new.
  - Core: `combat_cast()` ends the turn for a heal with no target. A cancelled crosshair with no aim
    now does nothing, where an aim-less Charm / Polymorph used to fall back on the nearest enemy.
  - Runtime: a refused cast is dispatched untargeted, so no prompt opens. The `Player:` caster is kept.
    A cancelled `On who:` dispatches the spent cast. A cancelled potion / scroll-6 target spends the
    item on every platform.
- **Before → after.**
  - A cancelled heal refunded everything; now it is spent.
  - `M.P. too low!` and the level gate's `Failed!` come before any target prompt.
  - Outside combat the `Player:` pick pays, not the active member.
  - **D-51 is retired.**
- **Kept (D-101 residual).** The circle ceremony still plays for a cancelled world heal: the live
  `gameplay_parity` row 2306 pins the reference's cast-time ceremony.
- **Tests.**
  - `spp1_runtime` F1-F6: 10 RED.
  - Mutants F1-F9 killed (F3/F8/F9 first survived; the test now asserts the picker never opened).
  - The `advanced_combat` v31 "cancel with a cell" rows keep the reference's rule.

### 3.3 A — the kernel dispatcher's turn

- **DOS.** K:0x3178 sets its result to 1 in its prologue (0x317e) and returns it, one world turn, on
  **every path** of these commands, including cancels, refusals and a getdir `Pass`:
  - (R)eady 0x339a, (Z)-stats 0x3472, (N)ew order 0x334e, (U)se 0x340c, (M)ix 0x3340;
  - (C)ast 0x3242;
  - the getdir commands L, G, O, J, S, P, F, T, X, and overworld K 0x32e8.

  Getdir 0x35EC has no ESC; Space is its only "no direction".

  The exceptions return 0, or a value of their own:
  - overworld A (MAINOUT 0x06ec) returns 0;
  - town A returns 1 except for `On foot!`;
  - D, Q, unknown keys and RETURN/ESC (`What?`) return 0.

  The loops then run one turn:
  - MAINOUT 0x0c39: `advance_clock(2)` and the turn block;
  - TOWN 0x15d0: `advance_clock(1)`;
  - DUNGEON 0x0f84: block 0x0c76.

  Native's old "free action" reading rested on `g_unk_24e6`, which is a repaint flag.
- **Layer.**
  - Core `CommandKind::DispatcherTurn` (appended) runs the context's own turn.
  - The runtime has one latch in `AlphaRuntime::route_input`. It is armed when such a letter is typed
    at the bare prompt, and settled when the interaction is over.
  - The latch charges once, unless the command:
    - charged its own turn;
    - changed the location or the dungeon state;
    - became a conversation, a shop or a fight.
  - The T-Deck's Cancel at a world getdir is ledger A-1's ESC, which the binary does not have. It
    counts as the Pass.
- **The conversation fix (`7728250d`).**
  - **What the differential found (§7.2).** The device asks `Your interest?` in TextEntry, never in
    `UiMode::Dialogue`, so the latch charged a turn when a town conversation ended.
  - **Why that is wrong for SPP1.** TALK 0x041c returns nonzero after a conversation. How the town loop
    treats the dispatcher's resulting 2 is unresolved, so SPP1 must leave the conversation's cost as it
    was.
  - **The fix.** The latch now also reads the dialogue session.
  - **Still charged.** Talk at an empty town cell is TALK 0x048a → 0x0491 `Nobody's here!` → 0 → the
    dispatcher's 1 → one town turn. Native still prints the overworld's `Funny, no response!` there
    (D-111). The test pins the turn, not the text.
- **The timer fix (`5ba2fa46`, found by the code review).**
  - **Problem.** The latch was settled only after an input. The device holds `Mixing...` for 10 ticks
    (A4-UI4 D-71, the dialogue pacer), so a completed Mix returned to the prompt on the clock, with no
    input to settle on. The next move or Pass then absorbed the Mix's turn as if it were the command's
    own: a completed Mix cost nothing.
  - **Fix.** `route_input` settles a leftover latch *before* the new input runs, as the 1988 loop ran
    the turn before reading the next key. On the screen the turn now lands at the next input, not at
    the end of the hold.
  - **Map reveal.** `at_command_prompt()` also excludes the map reveal, whose keys are swallowed. Each
    letter pressed during Death Vision's 1.1 s reveal used to cost a world turn.
- **Before → after.** Each of these commands now costs one turn of its context (2 minutes on the
  surface, 1 in a town or dungeon). That holds on every path, including a cancel. Nothing is charged
  twice.
- **Tests.**
  - `spp1_runtime` A1-A13: 20 RED against `c0e95816`; A11 2 RED against `c4f53082`; A12 / A13 2 RED
    against `37d20f27`. A12 runs paced, as the device does.
  - Mutants A1-A6 and A8-A10 killed. A7 is equivalent.
  - Updated pins, each contradicted by the binary:
    - batch14 Z11-4 / Z10-4 / Z14-2;
    - alpha_runtime_integration E-2;
    - a3_hf10 N12.8;
    - the a3_04f census and panel golden. States 0-150 are identical; the first difference is the Z
      cancel at state 151 (`spp1-a-golden-proof.log`).

### 3.4 C — the arena's J, P and Y

- **DOS.** COMBAT.OVL dispatches the three keys to the overworld's own routines. Each ends the actor's
  turn on every path; the getdir's Space (Original) is Pass and yields the turn.
- **J** (0x097a → SJOG 0x0d4a): the keys are tested before the getdir (`No Keys!`, 0x0d66).
  - Locked doors 0xB9 / 0xBB and stocks / manacles 0x84 / 0x85 draw `rand(0,29)` against DEX.
  - Magic locks always break the key.
  - An untrapped chest breaks the key with no draw. A trapped chest draws `rand(1,30)` against a
    byte-truncated threshold.
  - Anything else is `No lock!`.
- **P** (0x0994 → CMDS 0x161a, no RNG):
  - A live room plate on the target fires silently.
  - A target that is occupied (an actor, a corpse or blood, loot, a field) or not in the pushable set
    is `Won't budge!`. Bare floor is not pushable.
  - Otherwise the target is pushed over the fill, else pulled, else `Won't budge`. That is a different
    string, DS 0x4567, with no `!`.
  - 0x90 / 0xB4 turn to face.
- **Y** (0x09c0 → CMDS 0x1418): a word is `No effect!`; an empty answer is `Nothing`.
- **Layer.**
  - Core: `CombatAction::Jimmy / Push / Yell` and `CommandKind::CombatJimmy / CombatPush / CombatYell`,
    all appended. No corpus moves.
  - UiSession: j / p / y in the arena, a key-count mirror (`No Keys!` before any prompt), and the
    `Jimmy-North`-style echoes.
  - The T-Deck's Cancel at `Jimmy-` / `Push-` stays free: the arena's existing handheld contract, as for
    Get / Open / Search.
- **Tests.**
  - `spp1_core` J1-J11 / P1-P12 / Y1-Y2: 0/25 before the fix.
  - `spp1_runtime` C1-C6 in both modes: 13 RED.
  - Scenario `spp1_original_arena` (P / Y).
  - Mutants C1-C14 killed.

### 3.5 E — the dungeon Look, and D

- **DOS.** DNGLOOK.OVL 0x0000 is the kernel L arm's only dungeon callee. It runs:
  1. `Look...`;
  2. the 0x4988 `Player: ` pick;
  3. the darkness gate (`You see:\ndarkness.`, no direction asked);
  4. the dungeon Search's `Dir-` (Ahead / Here / Left / Right, Space = Pass, ESC ignored);
  5. `You see:` and the class text of the raw map byte;
  6. on a fountain, `Will you drink?` (Y/N only). Y applies the whole byte (0x50 cure, 0x51 heal,
     0x52 poison, else `Bad taste.` and `rand(0,7)`) to the member picked at step 2.

  K:0x3310 returns 1 on every path, so every Look costs one dungeon turn: `None!`, darkness, Pass and
  `No.` included. The binary has **no Drink command**: `D` is K:0x324e `D-What?`, with no turn. A
  fountain under the party is reachable as L, ↓ (Here), Y.
- **Layer.**
  - Core: `DungeonAction::Look / LookDrink`, `DungeonEventKind::DrinkPrompt`,
    `GameEventKind::DungeonDrinkPrompt`, all appended. `dungeon.txt` still pins the old `Drink` action.
  - UiSession: L and D in the dungeon, and the drink question.
  - Runtime: the `Player:` seam (`UiRequestId::LookMember / LookHereMember`, appended), the darkness
    gate, then `Dir-` or Here.
- **Before → after.**
  - **Original**: L is the 1988 Look, and D is `D-What?`.
  - **T-Deck and Enhanced**: L is the Look. D is kept by ledger E-3, **amended**: D is a shortcut for
    the Look aimed Here (L, ↓) and asks the same `Player:`. So who drinks and what it costs are the
    binary's rules. The invented `No fountain here.` is gone.
- **Tests.**
  - `spp1_core` E-C1..E-C7: all 256 bytes, 12 RED before the fix.
  - `spp1_runtime` E1-E7: 9 RED.
  - Scenarios `spp1_original` (Look, `Dir-Pass`, `D-What?`) and `spp1_enhanced` (D as Look-Here, L).
  - Mutants E1-E9 killed.

### 3.6 D — spell target prompts

- **DOS.**
  - **No prompt in the arena.** An Xen Corp (CAST 0x043e), In Por (0x05f3) and Wis Quas (0x074c) ask
    nothing: their arms call neither the crosshair (COMSUBS 0x0504) nor the getdir (CAST2 0x0306).
  - **An Ylem has a town arm** (CAST 0x0230), which asks `Direction-`:
    - the tiles 0x5b, 0x90-0x93, 0x9d, 0xa5-0xa6, 0xa8-0xa9 and 0xad-0xaf become 0x44 with `POOF!`;
    - any other tile is `Failed!`;
    - Space is silent.
  - **Line spells.** The four arena line spells read the one-step `Direction-` (CAST 0x1f8a).
  - **Creature prompt.** Charm and clone print `Creature: ` (DS 0x45bb / 0x45dc).
- **Layer.** Core `cast_target_prompt()` and `world_magic.cpp` (the volatile-tile pre-check, the town
  Poof). Original's `Direction-` / `Creature:` live in the runtime. The T-Deck and Enhanced keep the
  reticle for the line spells.
- **Tests.**
  - `spp1_runtime` D1-D4: 5 RED.
  - `batch5` A2: corrected. It had pinned "no An Ylem world effect".
  - Mutants D1-D5 killed.

### 3.7 B — the paid rumour (P2)

- **DOS.** After `Fair 'nuff?` Y and the payment, SHOPPES2 0x0636-0x065c prints these, in order:
  1. record 85 + `rand(0,3)`, with `&` the subject (DS 0x4CA8) and `*` the place (DS 0x4CDC / 0x4CF6);
  2. `\nsays ` + the keeper + `.\n\n` (DS 0x9F82 / 0x9F8A).
- **Before → after.**
  - Before: the native shop drew the `rand(0,3)` and discarded it, so no front end showed anything for
    the gold.
  - After: the record and the `says` line follow the draw. The RNG stream is unchanged.
- **Layer.** Core `shop_orchestration.cpp`: `item_name()` for `RumorDeal`, and the `RumorDeal` branch.
- **Tests.**
  - `spp1_core` P2: the text, and the form following the draw over 24 seeds.
  - `spp1_runtime` B1 / B2: a real Talk to Sam of Moonglow's Honest Meal in both profiles, served first
    (the rumour's T is only answered once served), with N as the control. B1 was RED in both
    profiles; B2, the control, passed before and after.
  - The core P2 checks were RED as well: 2 RED.
  - Mutants B1-B5 killed.

## 4. Deferred and unresolved

Each item is recorded in `docs/hardware/ALPHA2_PRESERVATION_LEDGER.md` §4. Where the new rows say
"reference first", a corpus generated from the TypeScript reference pins today's behaviour. The
reference must be fixed at the pinned layer, `--check` run as the control, the corpus regenerated, and
the change proven by a token diff before native follows.

| Row | What | Why deferred |
|---|---|---|
| **D-104** | Shop transaction semantics B S1-S19 (`B-shops.md` §2). For example: the buyer's INT is `characters[0]`, not the active / first able member; no Falsehood drain on a ship; the shipwright's `0x6605` state machine; inn time-spell clear, farewells, pickup guards / HP / months; tavern drink counter, active-player reset, an unpaid rumour ending the visit, negative ration counts; the mounted mask `0x12`; full stables; the healer's `HP == max`. S12 is D-96. | The shop corpora (`shop_parity`, `shop_flow`) pin the TS ShopConsole; reference first. |
| **D-105** | Shop presentation: P1, the DOS console shop presenter for Windows Original (lists, pitches and prices as console lines); P3 (the reagent cap before the pitch); P4 (Original's Y/N on rounds); P5 (each tavern's own letters, shipwright F/S, guild D); P6 (the shipwright always `sir`); P7 (`for the  of ye`). | A new presenter for the whole flow (P1, P4, P5); texts pinned by the shop corpus (P3, P6, P7). |
| **D-106** | Spell targeting remainder: Rel Hur's `Direction-` and chosen wind; Charm / Polymorph validity (hostility; Blackthorn / Lord British / Shadowlord immunity; `Failed!`); Rel Xen Bet's crosshair; the arena An Ylem / An Sanct / An Grav / An Ex Por / In Ex Por adjacency and effects; Mani `Success!` at full HP; An Zu's silent success. | `gameplay_parity` / `advanced_combat` corpora; reference first. |
| **D-101** (amended) | The ceremony for a cancelled world heal (the reference emits it at cast time, row 2306); the `Failed!` glide. | Reference first. The refund half and the U / C world turn are fixed (F, A). |
| **D-107** | Dispatcher-turn remainder: | — |
| | • the town conversation (TALK's nonzero return → dispatcher 2) keeps its old cost (none); | the town loop's handling of 2 is not traced |
| | • H (0x3288 returns 1, `Only in bed!` included); | not ported |
| | • B / E / Y overlay returns; | untraced |
| | • town and dungeon K (the overlay's ax). | untraced |
| | Related: D-91 (the dungeon loop's clock order) and D-94 (the town's invalid digit) are unchanged. | — |
| **D-108** | Ctrl-S prints no `Sound On/Off` echo and costs no dungeon turn. Ctrl-B (`Buffer On/Off`, the BIOS type-ahead flush) is deliberately unmapped. | Ctrl-S's echo path is untraced; Ctrl-B has no counterpart. |
| **D-109** | TypeScript reference divergences found on the way: E §5.3 T1-T4 (the TS dungeon Look uses the active member, makes `Player:` / Pass free, and runs the turn before the drink); C D1 / D2 (`main.ts`: ESC at J/S/P `Pass` spends the turn; Yell's ESC = `Nothing` + turn). | The native ports follow the binary. The reference needs its own batch. |
| **D-110** | Arena J/P/Y residuals: the chest `Key broke!` tone (SJOG 0x0c31); off-grid reads (the binary reads raw buffer bytes); whether corridor arenas carry live plates; D-100's 0x84 / 0x85 scheduler skip. | Not ported, or unknown. |
| **D-111** | A town Talk at an empty cell prints the kernel's overworld `Funny, no response!`; the binary prints TALK 0x0491's `Nobody's here!` (DS 0x91d6). This is Windows fidelity matrix §7 item 5, now given its evidence. SPP1 A charges its turn correctly. | Outside SPP1's seven families; the dialogue corpus may pin the string (reference first). |

The pinned-row map, D-90 – D-103:

| Row | Relation to SPP1 |
|---|---|
| D-91 | Unchanged. SPP1's dungeon turn is the existing orchestration (`DungeonAction::Tick`). |
| D-94 | Unchanged. A10 pins that a valid digit stays free; the invalid digit is still free. |
| D-96 | Is B S12. |
| D-100 | Is related to C: Jimmy opens stocks / manacles. |
| D-101 | Partly fixed (F, A). |
| D-90, D-92, D-93, D-95, D-97, D-98, D-99, D-102, D-103 | Untouched. |

## 5. What a player sees change

**On the T-Deck** (and Windows Enhanced), the device keys and screens being otherwise unchanged:

1. **Turns (A).**
   - Ready, Z-stats, New Order, Use, Cast, Mix and the direction commands now cost a turn, even when
     cancelled: Mic at a world `Look-` is the binary's Pass.
   - The clock moves (2 min on the surface, 1 in a town or dungeon), and NPCs and monsters get their
     move. In a town that can bring a guard's `Pay tribute?` one turn earlier.
   - A conversation still costs nothing. Talk at an empty town cell costs a town turn.
   - A completed Mix's turn shows at the next key, after the `Mixing...` hold. Keys pressed during a
     map reveal are swallowed and cost nothing.
2. **Cast and Use (F).**
   - A cancelled heal target spends the spell's charge and MP. In the arena it also ends the actor's
     turn (COMBAT 0x0923).
   - In the arena, Mic at the reticle (a cancelled crosshair) now makes any spell do nothing. The
     charge and MP are spent, and there is no nearest-enemy fallback: an aim-less Charm or Polymorph
     used to hit the nearest enemy. Under the handheld contract the T-Deck keeps that cancel free (the
     actor keeps its turn); Original yields it.
   - A cancelled potion / In Mani Corp scroll target spends the item; `Potion` / `Scroll` is echoed.
   - `M.P. too low!` / level `Failed!` come before the target prompt.
   - The `Player:` caster pays.
3. **Arena (C).** J, P and Y work (`Jimmy-`, `Push-`, `Yell`) and end the actor's turn. Mic at
   `Jimmy-` / `Push-` stays free.
4. **Dungeon (E).**
   - L is `Look...` → `Player:` → `Dir-` (trackball; Mic = Pass) → `You see: …`.
   - D is `Look...` → `Player:` → the party's own cell. On a fountain it asks `Will you drink?`; the
     picked member drinks.
   - Each costs one dungeon turn. `No fountain here.` is gone.
5. **Arena spells (D).** An Xen Corp, In Por and Wis Quas no longer show a reticle. In a town, An Ylem
   asks a direction and can `POOF!` a field or door.
6. **Tavern (B).** A paid rumour prints its gossip and `says <keeper>.`
7. **Ctrl-K / Ctrl-V (G).** Not typeable on the device.

**On Windows Original**, the 1988 interaction model, on top of the above:
- Ctrl+K / Ctrl+V.
- Space at a getdir is `Pass`, which spends the turn.
- The arena's `Yell what?` and the getdir's `Jimmy-Pass` / `Push-Pass`.
- The dungeon's `D-What?` and `Dir-Pass`.
- `Direction-` for the arena line spells, and `Creature: ` for charm / clone.

**On Windows Enhanced**: the T-Deck's rules with desktop keys, plus Ctrl+K / Ctrl+V.

## 6. Tests

| Gate | Result |
|---|---|
| Host suite (serial) | **206/206** after B, after the A11 fix and on the final tree (`spp1-b-ctest.log`, `spp1-a11-ctest.log`, `spp1-final-ctest.log`). Baseline 204; new targets `spp1_runtime` and `spp1_core`. |
| `spp1_runtime` (real AlphaRuntime and Board, both profiles) | **96/96** |
| `spp1_core` | **42/42** |
| Windows ctest | **29/29** with the three new SPP1 scenarios (§7.4); 26/26 before them. |
| RED-first (`native/core/tools/spp1_red_first.py`: each item's production files at the commit before it, under that item's tests. G against `e089f2c5`, F against `0b1fd6c0`, A against `c0e95816`, C against `5b9e877b`, E against `f8ad45cb`, D against `d049500c`, B against `dc6c38bd`, A11 against `c4f53082`, A12 / A13 against `37d20f27`) | G 7 + 4, F 10, A 20 + 2 + 2, C 25 + 13, E 12 + 9, D 5, B 2 + 2 |
| Mutation (`native/core/tools/spp1_mutation_check.py`) | 57 mutants: G1-G5, F1-F9, A1-A10 (A7 equivalent), C1-C14, E1-E9, D1-D5, B1-B5. All killed except A7. |
| Save / load | The pinned save suites stayed green (A4-SAVE1/2/3, batch26/27/28, a3_04g / a3_hf4). SPP1 adds no save field and no `GameState` byte. |
| Parity corpora (`gameplay_parity`, `advanced_combat`, `shop_parity`, `shop_flow`, `dungeon_parity`, `command_parity`, `quest_parity`, …) | Unchanged and green. The rule: an SPP1 change that would move a corpus row was deferred, not forced. |

## 7. T-Deck and Windows verification

### 7.1 Code audit

The T-Deck-visible branches are the ones in §5. Original-only code is behind
`platform_dos_interface()`, which is null on the device. No `GameState` or save byte was added, and
nothing in the firmware allocates on a new path.

### 7.2 T-Deck differential

The method follows the Windows fidelity audit (matrix §8.5); the tools are in
`native/core/tools/spp1_differential/` and the log is `native/core/spp1-tdeck-differential.log`. It
was run against the baseline worktree `C:\Dev\uvw-spp1-base` (`e089f2c5`).

- **The walks.** 1,044 walks and 8,376 raw T-Deck inputs through the real AlphaRuntime and Board:
  - every letter × 8 scripts, with and without an active player;
  - the arena, J / P / Y included;
  - every shop, including a served-then-rumour walk;
  - conversations;
  - towns and dungeons;
  - the dungeon L / D picks;
  - Ctrl codes;
  - menus, save and load;
  - shrines.
- **The first run** found one unexpected difference: a town conversation cost a turn when it ended.
  It was fixed in `7728250d` (§3.3).
- **The rerun.** 494 walks are byte-identical and 550 differ. `spp1_probe_classify.py` assigns each
  differing walk by its first divergence:

  | Item | Walks | What differs |
  |---|---|---|
  | A | 484 | Exactly one more turn after a command in the dispatcher's arm set; text, mode and prompt identical |
  | A* | 6 | The same town turn, then a guard's `Pay tribute?`. The baseline shows the same demand after its own town turns. |
  | F+A | 2 | `Use on whom?` cancelled: the potion is spent and echoed |
  | B | 2 | The rumour's gossip and `says Sam.` |
  | C | 10 | J / P / Y open `Jimmy-` / `Push-` / `Yell` instead of `What?` |
  | E | 36 | Dungeon L / D are `Look...` + `Player:` |
  | G | 10 | Raw 0x0B / 0x16 echo `60` / `1.16`, with a dungeon turn underground |
  | **Unexplained** | **0** | |

- **Controls.** The positive control, the same build under the DOS profile, differs in 1,033 walks. Two
  runs of the SPP1 build are byte-identical.
- **After the review fixes (`5ba2fa46`).** The unpaced rerun is byte-identical to the run above. A new
  `--paced` mode runs the device's own scene and dialogue pacers, as on hardware. Against a paced
  baseline it gives the same picture: 494 identical, 550 differing, 0 unexplained.
- **Limits.**
  - Each walk is classified by its first difference only. The per-item tests cover each change on its
    own.
  - The walks did not reach D's spells, a completed Mix or a map reveal; `spp1_runtime` D, A12 and A13
    cover them.

### 7.3 Firmware

ESP-IDF 6.1, the same `sdkconfig` (identical SHA-256 in all trees), `ninja -j 4`, `-Werror`. Not
flashed. The SPP1 image was configured at `5ba2fa46` (it embeds `FW 5ba2fa46b74f`); every later
commit changes only docs and logs.

| | Baseline `e089f2c5` | SPP1 | Δ |
|---|---|---|---|
| Image (`openu5_tdeck.bin`) | `0x104020` (1,064,992 B) | `0x105f10` (1,072,912 B) | **+7,920 B** |
| App partition free (1.25 MiB) | 245,728 B (18.7 %) | 237,808 B (18.1 %) | −7,920 B |
| Launcher allocation (64 KiB steps) | 1,088 KiB (the build reports the next step 49,121 B away, i.e. 49,120 B of headroom) | 1,088 KiB (41,201 B away, i.e. 41,200 B of headroom) | unchanged |
| Sections (`esp_idf_size --diff`) | | flash `.text` +7,244, `.rodata` +672; DIRAM `.bss` +16 (54,064 B), all in `main.cpp.obj`, which holds the static `AlphaRuntime`: its new members (the turn latch, the pending caster); `.data`, IRAM and PSRAM unchanged | +7,916 (map) |
| Stack (`-fstack-usage`, 3,020 → 3,030 functions) | largest frame 5,920 B | largest frame 5,920 B | 6 frames changed, 10 new |

`check_app_budget.py` warns below 128 KiB free and fails below 64 KiB. It passes with 237,808 B free, and the build is clean under `-Wall -Wextra -Wconversion -Werror`.

The stack changes:

- `AlphaRuntime::route_input` (656 B) became a 64 B wrapper around `route_input_body` (656 B),
  so the input path is +64 B. The wrapper calls `arm_dispatcher_turn` (32 B) and
  `settle_dispatcher_turn` (144 B) beside the body, not under it.
- `dungeon_action` +80 B (400 → 480), `ShopRun::confirm` +64 B (272 → 336), `combat_action`
  +48 B (272 → 320), `cast_selected_spell` +16 B and `run_dungeon_command` +16 B.
- The new leaves are at most 144 B.

Logs: `native/core/spp1-fw-size-diff.log` and `native/core/spp1-fw-stack-diff.log`; the build
logs are `native/targets/tdeck/spp1-fw-build.log` and `spp1-fw-baseline-build.log`.

### 7.4 Windows

- **Unit tests.** `uv_desktop_input_test` gained K1-K3 (Ctrl+K, Ctrl+V, and Ctrl+B unmapped).
- **Scenarios.** Three run the real executable headless:
  - `spp1_original`: G and E in Original;
  - `spp1_original_arena`: C's P / Y in Original;
  - `spp1_enhanced`: G and E's D-as-Look-Here and L in Enhanced.

  Against an `e089f2c5` executable built from the baseline worktree, all three are RED for the expected
  reasons: no `1.16`, no karma, `mode 2` instead of the `Push-` getdir, `No fountain here.` instead
  of `Look...` (`native/core/spp1-win-scenarios-red.log`). On SPP1 all three are GREEN.
- **Suite.** Windows ctest 29/29 (`native/core/spp1-final-windows-ctest.log`).
- **Test package.** `native/targets/windows/dist/UltimaV-Native-Windows-x64-SPP1-test.zip`, inside
  the parity worktree. The development ZIP in the Windows worktree is untouched. The package is built
  after the docs commit, because its asset-preparation manifest must name HEAD; its path, commit and
  SHA-256 are in `native/core/spp1-package.log`.

## 8. Hardware test needs (nothing here has run on a T-Deck)

Flash an SPP1 image to the T-Deck only after the owner decides to. Run each check from a state set
with the Developer rows: a party with keys, torches and MP; a dungeon fountain cell.

1. **Turn cost (A).** Overworld: R + Mic, Z + Mic, N + Mic, U + Mic, C + Mic, M + Mic and L + Mic each
   advance the clock by 2 minutes. In a town, by 1. A Talk with an NPC ended by an empty answer leaves
   the clock alone. Mix a spell (`Mixing...`, `Done!`), then press Enter (Pass): the clock moves by
   4 minutes. Cast a Death Vision and press letters during the reveal: the clock does not move.
2. **Arena (C).**
   - J with no key: `No Keys!`, and the turn passes.
   - J at a locked door: `Unlocked!` or `Key broke!`.
   - P against bare floor: `Won't budge!` (bare floor is not pushable).
   - P on a pushable object that can neither slide nor be pulled: `Won't budge`, with no `!`.
   - Y, then ORT: `No effect!`.
   - Mic at `Jimmy-`: free.
3. **Dungeon (E).**
   - L → pick → Up: `You see: …`.
   - D on a fountain: `Will you drink?` → Y. The picked member is healed or cured.
   - D elsewhere: the cell's description, and one turn.
   - In darkness: `darkness.`
4. **Cast / Use (F).**
   - Mani, then Mic at `On who:`: the charge and MP are spent.
   - A potion, then Mic at `Use on whom?`: the potion is spent, with `Potion` echoed.
   - A Mani with too little MP: `M.P. too low!` and no picker.
5. **Spells (D).**
   - In the arena, An Xen Corp / In Por / Wis Quas cast with no reticle.
   - In a town, An Ylem at a field: `POOF!`.
6. **Tavern (B).** In Moonglow, buy a drink, then T, `brit`, Y: `"Seek ye a daemon in the desert!"`
   (the form varies) and `says Sam.`
7. **Pacing.** Nothing new is paced. Watch for any glitch in the transcript after the dungeon Look's
   pick.

## 9. Commits

| Commit | What |
|---|---|
| `b7593ced` | The binary derivations (`native/core/spp1-findings/`) |
| `0b1fd6c0` | G |
| `c0e95816` | F |
| `5b9e877b` | A |
| `f8ad45cb` | C |
| `d049500c` | E |
| `dc6c38bd` | D |
| `c4f53082` | B (P2) |
| `7728250d` | A: the conversation latch; the T-Deck differential |
| `b984fee0` | Docs, ledgers, the SPP1 scenarios, the firmware figures |
| `af582375` | The first test package's log (built from `b984fee0`, before the review fixes) |
| `37d20f27` | Docs corrections from the documentation review |
| `5ba2fa46` | A: a timer-ended command's turn; no turns during the map reveal (the code review) |
| (this commit) | Docs and firmware figures for `5ba2fa46`; the package is rebuilt from it |

All RED-first, mutation, ctest and differential logs are under `native/core/spp1-*.log`. Mutant A8's
run is `spp1-a8-mutation.log`.

Erratum: `b7593ced`'s message says the CRLF baseline attempt "failed 10 drift tests". Its log
(`spp1-baseline-ctest-crlf-attempt.log`) records 20 failures, and §1 is right.
