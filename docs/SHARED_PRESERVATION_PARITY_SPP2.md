# Shared Preservation Parity, Batch 2 (SPP2)

**Branch** `parity/shared-preservation-2` · **worktree** `C:\Dev\ultima-v-native-spp2` · **base** `fc4a63aa`
(the SPP1 HEAD, unchanged) · **date** 2026-10-14 · **not pushed, merged, published or flashed.**

SPP2 follows SPP1 (`docs/SHARED_PRESERVATION_PARITY_SPP1.md`). It has three parts:

- it rechecks SPP1's Mix turn latch;
- it investigates SPP1's deferred rows D-104 – D-111;
- it corrects what has clear executable evidence and a small blast radius.

Every correction is in shared code: `native/core` and the shared application layer
`native/targets/tdeck/main`. The T-Deck and both Windows modes therefore get the same rules. The
hardware handoff for the **frozen SPP1 image** is the separate `docs/SPP1_HARDWARE_TEST_HANDOFF.md`.

| Status word | Meaning |
|---|---|
| **CONFIRMED** | Read in the DOS binaries at the cited offsets (`re/tools/dis16.py`, the install in `original/u5/ultima5`; the context loops and kernel are the 2001 *Upgrade 1.0* files, as in SPP1 §1). Static only: no DOS session was run. |
| **IMPLEMENTED + AUTO-VERIFIED** | The tests were RED before the fix and GREEN after, through the real AlphaRuntime and Board. Mutants were killed, the host and Windows suites are green, and the T-Deck differential has 0 unexplained walks. |
| **NOT HARDWARE-TESTED** | Every item. No T-Deck was used and nothing was flashed. |
| **RESEARCH ONLY / DEFERRED** | Evidence recorded and nothing changed. The reason is given. |

## 1. Provenance and baseline

- **Worktrees at the start**, all verified:
  - `C:\Dev\OpenU5-Native`: `main` at `96a55ff9`, four untracked A4-FINAL-HW files;
  - `C:\Dev\ultima-v-native-windows`: `windows/initial-port` at `e089f2c5`;
  - `C:\Dev\ultima-v-native-parity`: `parity/shared-preservation-1` at `fc4a63aa`, clean;
  - `C:\Dev\uvw-spp1-base`: detached at `e089f2c5`.

  There is no `C:\Dev\ultima-v-native`: the main checkout is `C:\Dev\OpenU5-Native`.
- **New worktrees**:
  - `C:\Dev\ultima-v-native-spp2` (this branch, from `fc4a63aa`, LF checkout);
  - `C:\Dev\uvw-spp2-base`, detached at `fc4a63aa`. It is used for the TypeScript FAIL-set baseline and
    the T-Deck differential's baseline probe. It is not the SPP1 worktree, which was never written to.
- **Ignored build inputs** were copied from the SPP1 worktree, and `node_modules`, `original` and
  `game/assets` are junctions: the same layout as SPP1.

| Baseline check (at `fc4a63aa`, in the SPP2 worktree) | Result | Log |
|---|---|---|
| Host suite, serial ctest | **206/206** | `native/core/spp2-baseline-ctest.log` |
| TypeScript `game/` vitest, full run (in `uvw-spp2-base`) | 116 failed / 7,927 passed / 211 skipped. These are pre-existing, environment-bound failures; FAIL sets are compared, never totals. | `native/core/spp2-vitest-failset.log` |
| SPP1 firmware | `build-spp1/openu5_tdeck.bin`, 1,072,912 B, embeds `Git 5ba2fa46b74f` | §8 |

## 2. Summary

| Item | Discrepancy | DOS evidence | Status | Commits |
|---|---|---|---|---|
| **M** | SPP1 charged a timer-ended command's turn at the **next input**. This covers Mix's `Mixing...` hold and the View scroll's map reveal. | MAINOUT 0x0c00 → 0x0c39 → 0x0d1a: the turn runs before 0x0b14 reads a key. Mix K:0x3340, CMDS 0x1b81-0x1c1f. | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED | `7b2ff6a5` |
| **N** | After a turn charged by the latch, an NPC's approach waited until after the next key's command, and a fight the turn began kept the surface song | TOWN 0x15bf on: the NPC tail runs in the same loop pass | CONFIRMED (control flow); IMPLEMENTED + AUTO-VERIFIED | `7b2ff6a5` |
| **T (D-111)** | A town Talk that opens no conversation printed the kernel's overworld `Funny, no response!` | TALK 0x048a → 0x0491 `Nobody's here!` (DS 0x91d6). The converse dispatch 0x031e gives `No response!` (0x0364, DS 0x9168) and `The guard offers no response!` (0x0387, DS 0x9148). | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED; TypeScript reference fixed at the pinned layer | `68743160`, `96dec6b6` |
| **G (D-108 + D-112)** | Ctrl-S had no `Sound Off/On` echo and no turn. RETURN/ESC (`What?`) at the **overworld** prompt cost nothing. | MAINOUT 0x0b80 / 0x0bb8 → 0x0c12 (result 1); TOWN 0x1508 / 0x1544 → 0x154b (0); DUNGEON 0x0776 → 0x070a (1) | CONFIRMED; IMPLEMENTED + AUTO-VERIFIED (Windows-visible only) | `5869fa20` |
| **H (D-107 (2))** | A cancelled Hole-up hours question, the ship repair and a completed Camp owe the dispatcher's turn | K:0x3288 → 0x3c9a / CMDS 0x0552 → 0x31ee; abandon 0x3ddc / 0x0587 | CONFIRMED; **DEFERRED** (it moves ~30 camp pins) | `b636a95a` (harness only) |
| D-104, D-105, D-106, D-107 (1)/(3), D-109, D-110 | see §4 | §4 | RESEARCH ONLY / DEFERRED | — |

The order of work was M/N → T → H (researched, deferred) → G → verification.

## 3. The Mix turn latch (SPP1 `5ba2fa46`)

### 3.1 What SPP1 does

SPP1's latch (`AlphaRuntime::arm_dispatcher_turn` / `settle_dispatcher_turn`) charges the kernel
dispatcher's turn when the bare command prompt returns. On the device, Mix ends inside the dialogue
pacer's 10-tick `Mixing...` hold (A4-UI4 D-71), so the prompt comes back on a timer. The trailing settle
therefore finds no prompt, and SPP1's fix (`5ba2fa46`) settles the leftover latch **at the start of the
next input**.

### 3.2 The binary

**CONFIRMED.** Mix is K:0x3340: `call 0x8082` (CMDS), then `jmp 0x31ee`, returning the prologue's 1.
CMDS 0x1b81 prints `Mixing...` (DS 0x8ff0) and 0x1b8f/0x1b98 waits 10 ticks (0x617a / 0x7b66). It then
deducts (0x1ba4), prints `Done!` (0x1bd6, DS 0x8ffc) or springs the trap (0x1bf6), redraws, and
returns (0x1c1f). MAINOUT then:

- stores the 1 (0x0c0f);
- tests it (0x0c30);
- runs `advance_clock(2)` (0x0c39), the hazards, housekeeping K:0x2ae8 and the world phase (0x0d11);
- jumps to the loop top (0x0d1a → 0x0a8f).

Only then does 0x0b14 read the next key. **The turn is on the screen before the player chooses the next
key.**

### 3.3 The questions asked

| Question | SPP1 (`fc4a63aa`) | Evidence |
|---|---|---|
| Does the clock advance when Mix completes? | **No**, only when another input arrives | spp2_runtime M1 RED: +0 after the hold |
| What if the player waits indefinitely? | The turn stays owed for as long as no key arrives | M2 RED: +0 after 10 s |
| Can time-dependent systems run before the charge? | Nothing in the game runs on its own while the T-Deck idles, so no rule can overtake the charge. **But the turn's own consequences are shown late:** the clock, the NPCs' and monsters' moves and an approaching NPC are absent from the screen the next key is chosen on. When the next key comes, the Mix's turn and that key's command run in the same input. | M1, N2 |
| Can a save or state snapshot see the pending turn? | Alt+S, Alt+L and the System Menu all go through `route_input`, whose leading settle charges first, so a save right after the hold holds the turn. **Alt+S inside the 550 ms hold** (the device shortcuts keep their meaning during pacer holds) saves the Mix without its turn. The live game still charges the turn once; loading that save charges nothing more. | M5 (both GREEN on SPP1 and SPP2) |
| Can a mode transition, the map reveal or a menu delay or discard the charge? | A menu or the Developer screen delays it, because `at_command_prompt()` is false. The title screen and a load discard it; the replaced game owes nothing. A teleport or a fight drops it by design. | code audit |
| Can the next command absorb the charge? | No. The leading settle runs before the new latch is armed. | M3 (GREEN on both) |
| Is anything charged twice? | No | M3, M6, H2 (control) |
| Is there a legitimate reason to delay? | No. The pacing is presentation of a busy loop the original also spends, and the original runs the turn at its end. | §3.2 |
| Map reveal (View scroll, Death Vision) | Same class as Mix: a timed hold that reads no key (R-12); U is K:0x340c → 1 | M4 RED on SPP1 |

**Verdict:** a genuine but small timing defect. The fix settles the latch at the command-completion
boundary, on the frame the hold ends. It adds no artificial input.

### 3.4 The fix (`7b2ff6a5`)

- **Settle on the frame.** `render()` calls `settle_dispatcher_turn()` right after the pacers are
  serviced. A turn owed by a timer-ended command therefore lands on the frame its hold ends, and that
  frame shows it.
- **Fallback.** `route_input` keeps its leading settle as the fallback: the map reveal is a plain
  timer, and an input can arrive before any frame has seen it run out (M6).
- **What the turn brings follows it.** After a latch-charged turn, `settle_dispatcher_turn()` now runs
  `drain_pending_npc_initiation()` and `sync_music()`, as `route_input_body` does after any routed
  command:
  - **N2.** SPP1 left an NPC's approach until after the next key's own command. In the binary, TOWN's
    NPC tail runs it in the same loop pass.
  - **N3.** SPP1 left a fight the turn began on the surface song until the next key: the A4-UI2
    render-driven-transition class.
- **Re-entrancy guard.** `command_depth_` keeps a latch from ever being settled inside `command()`. The
  only `render()` nested in a command is the unpaced Camp scene's.

### 3.5 Tests

`spp2_runtime` runs the real AlphaRuntime and Board, paced, with the device's raw keys:

| Test | What it checks |
|---|---|
| M1 | Completion with no input: +1 world turn, +2 min |
| M2 | A delayed Pass: two turns in all, and nothing more while idle |
| M3 | A key during the hold is swallowed; a Pass on the release frame is its own turn |
| M4 | The View scroll's reveal ends on its own and the turn is charged |
| M5 | Saves made before, inside and after the hold |
| M6 | A key that arrives before the reveal's first frame |
| N1 | Control: a Pass with an approaching NPC opens the conversation at once |
| N2 | An R cancel does the same |
| N3 | The fight's music after an R cancel beside a troll |

- **RED against `fc4a63aa`:** 8 checks (`spp2-m-red-first.log`). GREEN 20/20.
- **Mutants** (`tools/spp2_mutation_check.py M N`, `spp2-mn-mutation.log`):
  - M1–M4 and N1–N2 are killed. M3 moves the settle before the pacer, so the turn lands one frame
    late, and is caught.
  - N3 (the depth guard) is EQUIVALENT: no test can reach it, because Camp never arms the latch.
- **Updated pin, SPP1 A11b.** Its empty cell was (2,2), beside Tika of the Wayfarer Tavern, whose
  approach now opens at once (the N rule). The cell now avoids every actor. The check itself is
  unchanged, and the change is GREEN on SPP1 code too.

## 4. D-104 – D-111: inventory and findings

Definitions are from `docs/hardware/ALPHA2_PRESERVATION_LEDGER.md` §4 and the SPP1 record §4. **Platform
effect** means:

- **all**: the shared rule, on the T-Deck and both Windows modes;
- **Original**: Windows Original only;
- **Win**: both Windows modes, but not the T-Deck.

### 4.1 Implemented

**D-111 — TALK.OVL's own answers (T).** Platform effect: all. Gameplay unchanged (text only).

- **DOS.** K:0x33c2 sends a town Talk to TALK 0x041c (`0x33f9 call 0x8196`):
  - getdir Pass → returns 0;
  - no NPC at the target (0x048a, after the counter step 0x046b) → DS 0x91d6 `\nNobody's here!\n`
    (0x0491) → returns 0;
  - an NPC with dialog number 0, through the converse dispatch 0x031e:
    - `No response!` (0x0364, DS 0x9168);
    - for an object of type 0x70 (a guard, 0x0371) `The guard offers\nno response!` (0x0387, DS 0x9148),
      unless the AI of its served schedule slot is 4 (0x0350; the binary also resets that AI to 1 at
      0x0354, which is **not ported**).

  TALK's 0 makes the dispatcher return 1, so the town turn is charged, as SPP1 A already does.
  `Funny, no response!` (DS 0xa216) is the kernel's own overworld answer.
- **Native before.** `Funny, no response!` for all three cases (`dialogue_orchestration.cpp`).
- **TypeScript before.** The same (`main.ts` town Talk; `Game.talkTarget` returns null for both "no
  NPC" and "dialog 0").
- **Fix.**
  - Native: `dialogue_orchestration.cpp`.
  - Reference: `Game.talkRefusal()`, used by `main.ts`. Three `[D]` entries in `approved-strings.json`;
    the contexto-tap mirror test follows main.ts.
  - Corpus (`96dec6b6`): `generate-dialogue-fixtures.ts` mirrors main.ts and now asks `talkRefusal`.
    `68743160` alone left `dialogue_parity` RED, because seven flows, whose NPC had joined the party,
    talked to an empty cell. `--check` went RED as the control. The regenerated
    `build-dialogue/dialogue.bin` (git-ignored) differs in **exactly 7 uint32 tokens**, and each new value
    equals native's "actual" from the RED run (`spp2-t-dialogue-token-diff.log`).
- **Tests.**
  - `spp2_runtime` T1–T5 in both profiles: 5 RED against `7b2ff6a5` (`spp2-t-red-first.log`).
  - vitest `spp2-talk-refusal`: 5 RED (TypeError) before the reference fix.
  - Mutants T1–T5 and TS-T1–TS-T4 killed.
  - Updated pin: a3_hf5 N6.3 now expects `Nobody's here!` (its point, an immediate answer, is unchanged).

**D-108 — Ctrl-S, and D-112 (new) — the overworld `What?` turn (G).** Platform effect: Win
(Ctrl-S); Original (RETURN/ESC).

- **DOS** (CONFIRMED; jump tables decoded at MAINOUT 0x0bbe and TOWN 0x1552; DUNGEON 0x06c4 compares
  the key directly):

  | Key | MAINOUT (overworld) | TOWN | DUNGEON |
  |---|---|---|---|
  | Ctrl-S (0x13) | 0x0b80: `Sound ` + `Off`/`On` (DS 0x2b54; flag 0xa9ce), toggle; `jmp 0x0c12` with the loop result still 1 (0x0a94): **one world turn** | 0x1508: the same text; `jmp 0x154b`, [bp-0xa]=0: **free** | 0x0776: the same text; 0x070a returns the default 1: **one dungeon turn** |
  | Other control codes (RETURN 0x0d, ESC 0x1b, …) | 0x0bb8: `What?` (DS 0x2b64) → 0x0aba → 0x0c12: **one world turn** | 0x1544: `What?` → 0x154b: **free** | ESC goes to the dispatcher (0x07bc → 0x07a0): `What?` = 0, free; RETURN is a movement key (0x07a8) |
  | Ctrl-K / Ctrl-V | 0x0b75 → 0x0af8 ([bp-8]=0): free | 0x154b: free | 1: a turn (SPP1 G) |

- **Correction to SPP1** (dated 2026-10-14). SPP1 A10 asserted that Original's RETURN/ESC "stay free"
  on the surface. It read them as the dispatcher's 0x34d8 `What?`, which only the dungeon reaches. On
  the overworld, MAINOUT handles control codes before the dispatcher and charges the turn. A10 now
  expects a world turn each, with a dated note. The town stays free (spp2_runtime G7), which matches the
  Windows `original_interface` scenario: it runs in a dwelling and is unchanged.
- **Native before.**
  - Windows Ctrl+S was the session SFX mute (A3-05): `SFX muted.`, no echo, no turn.
  - Original's RETURN/ESC printed `What?` in UiSession, with no turn anywhere.
- **Fix.**
  - `desktop_input.cpp`: Ctrl+S emits the appended `DeviceShortcut::DosSound`. F8 and Alt+Shift+S stay
    the plain mute.
  - At the command prompt, `AlphaRuntime` toggles the mute without its session line and sends
    `ControlKey 0x13` (member = the sound was on). Elsewhere, Ctrl+S is the mute alone.
  - The core `ControlKey` handles 0x13, 0x0d and 0x1b: the echo or `What?`, and the context's turn rule.
  - UiSession's Original RETURN/ESC at the overworld/town prompt send 0x0d / 0x1b.
  - The T-Deck has no Ctrl and keeps Alt+Shift+S as the mute (G5 control). Its Enter is still the Pass
    (G8 control).
- **Tests.**
  - `spp2_runtime` G1–G8: 9 RED against `96dec6b6`, plus SPP1 A10 (`spp2-g-red-first.log`).
  - Mutants G1–G7 killed.
  - `win_dos_interface` E4 now pins the control codes sent.
  - Windows scenarios `spp2_original` / `spp2_enhanced`: 7 + 3 RED on the SPP1 executable
    (`spp2-win-scenarios-red.log`), GREEN here.
  - `uv_desktop_input_test`: Ctrl+S → DosSound.
- **Not ported.**
  - Backspace and the other unhandled control codes at Original's overworld prompt (DOS: `What?` + a
    turn) still do nothing.
  - MAINOUT's "Exit to DOS? N" turn has no counterpart.

### 4.2 Researched and deferred

**D-107 (2) — Hole up.** Platform effect: all. CONFIRMED.

- **DOS.** K:0x3288:
  - outside a town, `call 0x3c9a` (camp; aboard ship, the repair) then `jmp 0x31ee`;
  - in a town on a bed, `call 0x802e` → CMDS 0x0552, then `jmp 0x31ee`;
  - off a bed, `Hole up- Only in bed!` (0x33ea → 0x31ee).

  Every path returns 1. Abandoning the hours question ('0'/Space; camp 0x3ddc/0x3de5 → 0x3eea `ret`;
  bed CMDS 0x057e/0x0587 → 0x06e8) returns through the same 1.
- **Native.**
  - The refusal already charges its turn (the core's Rest, `r.turn()`). Ledger D-107's "`Only in bed!`
    is not charged" is stale.
  - A cancelled hours question (RestCancel), the ship repair and a completed Camp charge nothing extra.
  - A bed rest increments the turn counter itself.
- **Why deferred.** Adding `h` to the latch's arm sets fixes the cancel and the ship repair (H1, H3–H6),
  but it also gives a completed Camp the binary's trailing world turn. That moves ~30 checks in
  batch32 (W5, C3), batch36, batch38 and batch44 (`spp2-h-camp-pins.log`). Their "no final housekeeping
  pass" was derived from CMDS's camp alone, and their RNG-trace pins need re-deriving with the
  dispatcher's turn appended. That is outside SPP2's low-risk scope.
- **What remains.** The change was reverted. H1–H7 stay as an opt-in harness (`spp2_runtime … --deferred-h`,
  RED by design, `spp2-h-deferred.log`). The bed's trailing turn would also need a rule beyond the
  latch, because the bed rest charges its own turns.

**D-107 (1) — a town conversation.** Platform effect: all. **Now traced** (CONFIRMED control flow;
0x1352's purpose PROBABLE).

- **DOS.** K:0x33f9 TALK → nonzero → `[bp-2]=2` (0x3403). TOWN then:
  1. 0x159d: not 3;
  2. 0x15b6: nonzero, so the turn block runs;
  3. `advance_clock(1)` (0x15d0);
  4. 0x0f02, with housekeeping K:0x2ae8;
  5. the object pass 0x0c78;
  6. result ≥ 2 **skips** the NPC.OVL movement (0x1662);
  7. 0x1671: result == 2 → TOWN 0x1352(1).

  So a conversation costs one town minute and a housekeeping pass, with 0x1352(1) in place of the NPC
  movement.
- **Native.** Free (SPP1 kept it so).
- **Why deferred.** Porting it needs a town turn that skips NPC movement and runs 0x1352, which native
  does not model. Charging the ordinary DispatcherTurn would move NPCs the binary doesn't move. Every
  conversation would also change cost on every platform.

**D-107 (3) — B / E / Y overlay returns; town / dungeon K.** Still untraced.

**D-104 — shop transaction semantics (S1–S19).** Platform effect: all. CONFIRMED in SPP1
`spp1-findings/B-shops.md` §2. **Deferred:**

- every row changes prices, gold, RNG or state that the `shop_parity` / `shop_flow` corpora pin from
  the TypeScript ShopConsole, so each needs the reference fixed at the pinned layer first;
- several (S5 the shipwright's 0x6605 machine, S8 guards on a failed pickup) are not small.

The smallest are S17 (mounted mask 0x12 vs 0x10), S19 (healer `HP == max`) and S6 (inn clears the time
spell). Each is a one-line native change, but each needs a reference change and a corpus regeneration.

**D-105 — shop presentation (P1, P3–P7).** Platform effect: P1/P4/P5 Original; P3/P6/P7 all.
**Deferred:**

- P1, P4 and P5 are a new DOS console presenter for the whole shop flow. That is a UI rewrite, and the
  brief excludes it.
- P3, P6 and P7 are texts pinned by the shop corpus (reference first).

**D-106 — spell targeting remainder.** Platform effect: all. CONFIRMED in SPP1
`spp1-findings/DF-spell-targeting-heal-cancel.md` §3–§6. **Deferred:** `gameplay_parity` /
`advanced_combat` pin the reference's behaviour. The rows:

- Rel Hur always calms the wind (it needs a `Direction-` prompt and a direction-to-wind map);
- Charm / Polymorph have no hostility or immunity test, and an aim-less cast picks the nearest enemy;
- Rel Xen Bet has no crosshair;
- the arena adjacency spells;
- Mani `Success!` at full HP;
- An Zu's silent success.

Rel Hur is the most gameplay-relevant of these. It also adds a T-Deck prompt.

**D-109 — TypeScript reference divergences** (the dungeon Look T1–T4; main.ts J/S/P ESC and Yell's
ESC). Reference only, with no native effect. **Deferred** to a reference batch: these flows are pinned by
TS unit and e2e tests that would need the same evidence-led rewrite. SPP2 fixed one reference
divergence (D-111) at its layer.

**D-110 — arena J/P/Y residuals** (the chest `Key broke!` tone at SJOG 0x0c31; off-grid reads; corridor
plates; D-100's 0x84/0x85). **Deferred.** The tone would change device audio, which this batch keeps
unchanged; the others are UNKNOWN or need a trace.

### 4.3 New rows found by SPP2

| Row | What | Status |
|---|---|---|
| D-112 | The overworld's control-code `What?` (RETURN, ESC, …) and Ctrl-S cost a world turn (MAINOUT 0x0bb8/0x0b80 → 0x0c12) | Implemented (G). Backspace and the other unhandled codes are still not ported. |
| D-113 | Talking to an NPC whose served schedule AI is 4 resets it to 1 (TALK 0x0354); native resets it only on the NPC-initiated approach (`blackthorn_turn_effect`) | Research only |
| D-114 | TALK's dispatch of dialog 0xFF calls the guard talk 0x01e2 in **every** town. On an even served slot the guard test answers `The guard offers no response!` first. Native hands off to the guard flow only in Blackthorn's palace (0x12) and otherwise prints `Funny, no response!`. | Research only |
| D-115 | A conversation's town turn (D-107 (1), traced above) | Research only |
| D-116 | A completed Camp, a bed rest and the ship repair owe the dispatcher's trailing turn; a cancelled hours question owes it too (D-107 (2)) | Deferred, with the harness |

## 5. What a player sees change (vs SPP1)

**On the T-Deck**, with everything else unchanged:

1. **Mix and the View scroll's reveal.** The world turn (+2 min, the world's moves) lands when
   `Done!` / the reveal ends, not at the next key.
2. **An NPC's approach.** When a turn the latch charges (an R/Z/N/U/C/M or direction-command cancel, in
   a town) brings an NPC's approach, the conversation or shop opens at once. A fight such a turn begins
   switches to the battle music at once.
3. **Talk in a town.** Talk at an empty town cell prints `Nobody's here!`. Talking to a dialog-0 NPC
   prints `No response!` (animals and the like) or `The guard offers no response!` (guards). The turn
   cost is unchanged.

**On Windows Enhanced:** the same, plus Ctrl+S, which prints `Sound Off` / `Sound On` at the command
prompt with that context's turn (overworld 2 min, town none, dungeon 1). Elsewhere Ctrl+S is the mute
alone.

**On Windows Original:** the same as Enhanced, plus RETURN and ESC at the overworld prompt (`What?`):
each costs a world turn. In a town they stay free.

## 6. Tests

| Gate | Result | Log |
|---|---|---|
| Host suite (serial, GCC 16) | **207/207**: the baseline 206 plus `spp2_runtime` | `native/core/spp2-g-ctest.log` (also `spp2-m-ctest.log` after M/N) |
| `spp2_runtime` | **43/43** (M1–M6, N1–N3, T1–T5, G1–G8). With `--deferred-h`: H2/H7 GREEN, H1/H3–H6 RED by design | `spp2-g-green.log`, `spp2-h-deferred.log` |
| `spp1_runtime` | **96/96**, with A10 corrected and A11b's cell moved | — |
| RED-first (`tools/spp2_red_first.py`) | M against `fc4a63aa`: 8. T against `7b2ff6a5`: 5. H against `68743160`: 5 (deferred). G against `96dec6b6`: 9 + SPP1 A10. | `spp2-{m,t,h,g}-red-first.log` |
| Mutation (`tools/spp2_mutation_check.py`, `tools/spp2_ts_mutation_check.py`) | 18 native mutants killed (M1–M4, N1–N2, T1–T5, G1–G7) and 1 equivalent (N3, defensive); 4 reference mutants killed (TS-T1..4) | `spp2-{mn,t,g}-mutation.log`, `spp2-t-ts-mutation.log` |
| Windows ctest (MSVC, real executable) | **31/31**: 29 plus `spp2_original` / `spp2_enhanced` | `spp2-final-windows-ctest.log` |
| TypeScript vitest, full | FAIL set **identical** to the baseline (116 = 116, the same ids); +5 new passing tests; `tsc --noEmit` clean | `spp2-vitest-failset.log`, `spp2-t-tsc.log` |
| Parity corpora | Unchanged except `dialogue_parity`'s 7 tokens (§4.1), regenerated from the fixed reference | `spp2-t-dialogue-token-diff.log` |
| Save / load | No save field or `GameState` byte was added; the pinned save suites are green | — |

## 7. T-Deck and Windows verification

### 7.1 Code audit

| Change | On the device |
|---|---|
| M/N: the frame-time settle, the drain and the music sync | Device-visible on purpose (§5) |
| T | Device-visible on purpose: text |
| G: `DosSound` | Never produced by the T-Deck adapter |
| G: the core's 0x13 / 0x0d / 0x1b arms | Reached only from the desktop shortcut and from UiSession's `dos` branch |
| `toggle_audio_mute`'s new `announce` parameter | Defaults to the old behaviour |

### 7.2 T-Deck differential

The method is SPP1's (`tools/spp1_differential`, matrix §8.5).

- **The probe** is `tools/spp2_differential/tdeck_diff_probe_test.cpp`, the SPP1 probe plus walk **I**.
  Walk I covers: a Mix to completion with three endings; the View scroll; Hole up cancelled in the world
  and in a town; a town Talk at an empty cell; and an R cancelled beside an approaching NPC.
- **The builds.** The probe was built in `uvw-spp2-base` (`fc4a63aa`) and in SPP2 (`5869fa20`), with a
  temporary CMake registration that was removed afterwards.
- **The classifier** is `tools/spp2_differential/spp2_probe_classify.py`. It assigns each differing walk
  by its first divergence. A T or M walk must also end on the same turn count and clock.

| Run | Walks / steps | Identical | Differing | Classified |
|---|---|---|---|---|
| Unpaced | 1,052 / 8,430 | 1,038 | 14 | T 12 (11 × the shop-0x86 Talk at no one, plus walk I), M 1 (the reveal), N 1; **unexplained 0** |
| Paced (the device's pacers) | 1,052 / 8,430 | 1,035 | 17 | T 12, M 4 (three Mix endings and the reveal), N 1; **unexplained 0**. One M walk ends on WAIT with no key after the hold, so SPP1 still owes the turn there, by design. |

- **Controls.**
  - Two SPP2 runs are byte-identical.
  - The positive control (the SPP2 build with the DOS profile) differs in 1,041 walks.
  - Hole up (deferred) and the raw Ctrl-K/V walks are identical, as expected.
- **Log:** `native/core/spp2-tdeck-differential.log`. The raw outputs are kept outside the repository.

### 7.3 Firmware

ESP-IDF 6.1, the same `sdkconfig` as SPP1 (identical SHA-256), `idf.py --no-ccache reconfigure` and
`ninja -j 4`, `-Werror`. The first attempt was clean, with 0 warnings. **Not flashed.**

| | SPP1 (`build-spp1`, `5ba2fa46`) | SPP2 (`build-spp2`, `5869fa20`) | Δ |
|---|---|---|---|
| Image | `0x105f10` (1,072,912 B) | `0x106380` (1,074,048 B) | **+1,136 B** |
| App partition free (1.25 MiB) | 237,808 B (18.1 %) | 236,672 B (18.1 %) | −1,136 B |
| Launcher allocation | 1,088 KiB (next step 41,201 B away) | 1,088 KiB (next step 40,065 B away) | unchanged |
| Sections (`esp_idf_size --diff`) | | flash `.text` +1,000, `.rodata` +128; DIRAM `.bss` 54,064 B, `.data`, IRAM and PSRAM **unchanged** | +1,128 (map) |
| Stack (`-fstack-usage`, 2,414 functions in each) | largest frame 5,920 B | largest frame 5,920 B | one frame changed: `AlphaRuntime::command` 1,728 → 1,744 (+16, the depth guard); two signature changes of 144 B each |
| Embedded identity | `Git 5ba2fa46b74f` | `Git 5869fa202109` | — |

- **Budget.** `check_app_budget.py` passes.
- **Audio and pacing.** No pacer duration or audio cue changed. The music sync after a latch turn only
  picks the battle song sooner.
- **Packs and saves.** Packs unchanged (no new SD pack); saves compatible both ways.
- **Logs:** `native/targets/tdeck/spp2-fw-{configure,build,size,size-diff,stack-diff,budget}.log`.

### 7.4 Windows test package

- **The package.** `native/targets/windows/dist/UltimaV-Native-Windows-x64-SPP2-test.zip`, inside the
  SPP2 worktree. It is 2,279,964 B, SHA-256 `c4384afd7a1789d52cf9bb414fab5903b3c3ca441f0bca2d0a8e1b66ff15983f`.
- **How it was built.** `tools/build-windows.ps1 -Config Release -BuildDir …\build-spp2 -Package
  -PackageName UltimaV-Native-Windows-x64-SPP2-test`, after `0bfa8e3a`, so the asset-preparation bundle's
  manifest names that commit. Log: `native/core/spp2-win-package.log`.
- **Tests.** The packaged build passes 31/31 (`spp2-package-windows-ctest.log`).
- **Other packages are untouched.** SPP1's `UltimaV-Native-Windows-x64-SPP1-test.zip` (SHA-256
  `dc5f3c3e…10de`) and the Windows worktree's development ZIP are unchanged.
- **Not a release.** No person has played it.

## 8. Remaining risks

- **Hardware.** No hardware run. The frame-time settle runs a game turn from inside `render()`. That is
  the same path the dialogue-pacer drain already uses (`drain_pending_npc_initiation` →
  `BeginConversation`), but a device session should watch for a hitch on the frame after `Done!`.
- **Saves during a hold.** Alt+S inside a timed hold saves the state before the turn (§3.3, M5). DOS
  cannot save mid-command; the device keeps its shortcuts there. This is recorded, not changed.
- **Binary version.** The control-key turns (D-112) rest on the 2001 Upgrade 1.0 MAINOUT (SPP1 §1
  caveat). A pristine 1988 MAINOUT was not available.
- **H stays partial.** Refusals are charged, but cancels, the repair and the trailing turns are not.

## 9. Recommended next batch (SPP3)

1. **D-116 / D-107 (2): Hole up.**
   - Re-derive the camp batches' pins with the dispatcher's trailing turn (batch32/36/38/44).
   - Add a rule for the bed's trailing turn.
   - Then enable `h` in the latch. The harness is ready: `--deferred-h`.
2. **D-115 / D-107 (1): the conversation's town turn.** Trace TOWN 0x1352, then model the
   "no NPC movement, 0x1352(1)" turn.
3. **D-106: Rel Hur's direction and wind.** It is gameplay-relevant. Reference first: `gameplay_parity`.
4. **D-104: the one-line shop rules** (S17, S19, S6), with the shop reference and corpus regenerated
   together.
5. **D-109:** a TypeScript reference batch for the dungeon Look and the getdir ESC rules.

## 10. Commits

| Commit | What |
|---|---|
| `7b2ff6a5` | M/N: the timer-ended turn at the hold's end; the turn's approach and music follow it |
| `68743160` | T (D-111): TALK's own answers (native + reference) |
| `b636a95a` | H (D-107 (2)): researched, deferred; the opt-in harness |
| `96dec6b6` | T: the dialogue corpus generator follows `talkRefusal` (fixes `dialogue_parity`, left RED by `68743160`) |
| `5869fa20` | G (D-108, D-112): Ctrl-S at the context prompt; the overworld `What?` turn |
| `0bfa8e3a` | This record, the SPP1 hardware handoff, the differential tools and logs, the firmware logs |
| (this commit) | The SPP2 Windows test package's record (§7.4) |

## 11. Follow-up findings on the SPP1 record (dated 2026-10-14)

The SPP1 document is not edited. Its readers should know:

1. **§3.3, the timer fix.** "On the screen the turn now lands at the next input" is SPP1's behaviour.
   The binary runs it before the next key is read: SPP2 M.
2. **§3.3 / §4, D-107.** "H … `Only in bed!` included is not charged" is stale: native's Rest refusal
   charges its turn. The town loop's handling of TALK's 2 is now traced (§4.2).
3. **§3.3 A10.** Original's RETURN/ESC at the **overworld** prompt cost a world turn (MAINOUT
   0x0bb8 → 0x0c12). The test is corrected with a dated note.
4. **§8 item 1.** The checklist's "the clock moves by 4 minutes" after a Mix and a Pass is right for the
   SPP1 image. `docs/SPP1_HARDWARE_TEST_HANDOFF.md` row 6 says what to expect on that image.
