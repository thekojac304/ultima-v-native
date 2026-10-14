# DF — Spell targeting fidelity (candidate D) and cancelled healing spells (candidate F)

Read-only investigation, 2026-10-14, worktree `C:\Dev\ultima-v-native-parity` (branch
`parity/shared-preservation-1`, head `e089f2c5`). No tracked file was edited, nothing was built,
and nothing was run in `native/core/build-spp1`.

## 0. Method and confidence key

- Binaries: `original/u5/ultima5/` (Exodus Upgrade 1.0 install). Disassembled with capstone through
  a scratch wrapper (`%TEMP%\spp1-D\u5d.py`, outside the repo) that resolves overlay near calls with the
  fitted bases from the brief (CAST/CMDS/SJOG/TALK 0xBF80, CAST2/ZSTATS/COMSUBS 0xE1E0, COMBAT/OUTSUBS
  0xA290, TOWN/MAINOUT/DUNGEON 0x81D0). It follows PLINK thunk records (`9a ec 02 2e 07 | id | ea tgt`)
  to the overlay. The overlay ids were fitted independently (`ovlmap.py`: 7 = COMBAT, 16 = CAST,
  18 = CAST2, 20 = COMSUBS, and so on, each by prologue hits). Every resolved call landed on a
  `55 8b ec` prologue or a thunk record unless noted. DATA.OVL strings were read at DS + 0x10.
- Offsets are FILE offsets of the named overlay, and ULTIMA.EXE offsets are image offsets (after the
  MZ header). "K 0xNNNN" is a kernel (ULTIMA.EXE) address.
- **CONFIRMED** = read instruction by instruction in this session. **PROBABLE** = a static reading with
  one link not closed, or not run. **UNKNOWN** = not settled.
- I re-derived every claim I took from a secondary note. Where a note was wrong, §9 says so.

---------------------------------------------------------------------------------------------------

## 1. The binary's Cast pipeline (one routine for world, town, dungeon and arena)

`CAST.OVL 0x0dba` is the only Cast routine. The kernel `C` arm (K 0x3242 → `call 0x7e5a` at K 0x3249)
and the arena (`COMBAT.OVL 0x095e`, thunk → CAST 0x0dba) both enter it. CONFIRMED.

| step | offsets | what happens |
|---|---|---|
| init | 0x0dc1-0x0dd2 | `[bp-6]` (return) = 1, `[bp-0xc]` (location ok) = 1, `[bp-0xa]` (res) = 1 |
| caster | 0x0dd5 `call K 0x4988` | `Player: ` picker (in the arena it is `g_cmb_actor` with no prompt); < 0 → exit 0x11d9 |
| name | 0x0de2-0x0e07 | `Spell name:\n:` (DS 0x4603), CAST2 0x00de getstring; -1 → `None!` (DS 0x4611), -2 → `No effect!` (DS 0x4618), both exit via 0x11d9 |
| circle | 0x0e0a-0x0e14 | `[bp-8] = idx/6 + 1` |
| location gate | 0x0e1a-0x0eb5 | mask `DS 0x1c90[idx]`: outdoors bit 8, arena (>0x7f) bit 1, town (<0x21) bit 4, dungeon bit 2; loc 0x12 without crown / 0x1d → `Absorbed!` (DS 0x4624) + tone_sweep; refused → `Not here!` (DS 0x462f) + glide K 0x43ae. **Both exit via 0x11d9 with nothing consumed** |
| None mixed | 0x0eb8-0x0ec5 | `g_spell_qty[idx]` (0x57f0+idx) == 0 → `None mixed!` (DS 0x463a), exit, nothing consumed |
| **spend charge** | **0x0ec8** | `dec byte [idx+0x57f0]`, unconditional |
| MP gate | 0x0ecc-0x0eea | MP `[caster<<5 + 0x55b7]` < circle → `M.P. too low!` (DS 0x4647), res = 0 → tail (`Failed!`); charge already spent, MP kept |
| **spend MP** | **0x0ef8** | `sub [si+0x55b7], al` (MP -= circle) |
| level gate | 0x0efc-0x0f0a | res = 0xFFFF; level `[si+0x55be]` < circle → res = 0 → tail `Failed!` (charge and MP spent) |
| arm | 0x0f0c-0x0f1a | `jmp cs:[bx-0x2f3a]`, table at file 0x1146 (48 words, minus 0xBF80) |
| **target prompt** | inside each arm | **every target prompt comes after every gate and after both deductions** |
| tail | 0x11a6-0x11d6 | res == 1 → `Success!` (DS 0x4656); res == 0 → `Failed!` (DS 0x4660) + glide; any other value (e.g. -1) → silent; `mov ax,[bp-6]` → returns 1 |

**Turn.** The kernel `C` arm stores the cast routine's AX (`K 0x324c jmp 0x3231; mov [bp-2],ax`).
After any arm the routine returns `[bp-6]` = 1, so a cancelled target prompt also returns 1. On foot,
`MAINOUT 0x0c30-0x0c3d` calls `push 2; call K 0x4f7c` (advance_clock) when the result is non-zero.
In a town, `TOWN 0x15b6-0x15d0` does `push 1` and the same call. CONFIRMED. The dungeon consumer was not
traced (UNKNOWN, as in MANI-FINAL §7.1).

In the arena, `COMBAT 0x0923` sets `[bp-2] = 0` **before** the absorb test and the cast. At 0x0b56,
`[bp-2] == 0` → 0x0b5f, the end of the actor's turn. So every Cast outcome (absorbed, refused,
cancelled, failed, success) ends the actor's turn. CONFIRMED (the "Can't!" and COMSUBS 0x09fc exits are
earlier and do not apply).

## 2. The binary's target input primitives

| primitive | code | prompt (DS) | keys | geometry | returns |
|---|---|---|---|---|---|
| **On who:** | CAST2 0x009e → K 0x2e8e → K 0x2d7a(0) | `On who: ` (0x94f4); name echoed on the row; ESC → `None!` (0x94fe) | roster picker: digits/arrows move, RETURN/Space choose, ESC = -1 (fidelity doc §2) | any party member, any status | roster index or -1 |
| **Direction-** (spell getdir) | CAST2 0x0306 (its own routine, not K 0x35EC) | `Direction-` (0x9504), then `North`/`East`/`South`/`West` (0x9510/0x9518/0x951e/0x9526) or `Pass` (0x952c) | **one key**: 1 = W, 2 = E, 3 = N, 4 = S (0x034f-0x03a5); Space → `Pass`, returns 0 (0x03a8-0x03b1); **every other key, ESC included, is re-read** (0x0367 → 0x037b → 0x0347) | origin = `g_cmb_actor`'s cell in the arena (0x030e-0x032b), the party's cell elsewhere; **[0x5876]/[0x5878] = origin + exactly one step** | 1..4, or 0 for Pass. Never negative |
| **Aim! / Creature:** crosshair | COMSUBS 0x0504(range, actor) | caller prints `Aim! ` (0x9a7e ranged COMSUBS 0x0a9a / 0x9a84 melee 0x0ccb) or `Creature: ` (0x45bb / 0x45d1 / 0x45dc) | arrows 1-4 and diagonals 0xd3-0xd6 move (0x064d-0x0719); a move is refused if `floor(sqrt(dx²+dy²))` (COMSUBS 0x048a/0x0458) > range, or outside 0..10; RETURN/`A` confirm, **ignored on the actor's own cell** (0x06a3-0x06b7); Space confirms, but **cancels on the actor's own cell** (0x068a-0x069e); ESC cancels (0x06ea) | the cursor starts on the actor's last target if alive and in range, else on the actor (0x0539-0x0565) | distance > 0 (confirm), 0 (cancel); cell in [0x5899]/[0x589a] |
| **To phase:** | CAST 0x0cff-0x0d1d | `To phase: ` (0x45e7) | one getkey (K 0x266c); printable echoed; `1`-`8` valid, anything else → res 0 (`Failed!`) | aboard ship → skipped (0x0cf6) | — |

The spell crosshair range is `DS 0x1664[weapon]` = **15** for every spell weapon 0x30-0x36, and 15 for
`Creature: ` (pushed literally at 0x09b4 / 0x0a70 / 0x0b3d). The largest distance on an 11 × 11 arena is
floor(sqrt(200)) = 14, so **every cell is reachable**. CONFIRMED.

## 3. Binary target table, grouped by model

Locations come from `DS 0x1c90` (file 0x1ca0), dumped this session. In the "where" column, O =
outdoors, T = town, D = dungeon, A = arena.

### 3.1 `On who:` (party roster) — heal family

| # | spell | where | arm → routine | valid target | invalid target | success |
|---|---|---|---|---|---|---|
| 2 | An Zu | OTDA | 0x0f3c → CAST 0x0114 | status `S` (0x0135) | 0 → `Failed!` | `S`→`G`, wakes the arena record (K 0x6800), ceremony 1, **returns -1 → silent** (0x0183) |
| 3 | An Nox | OTDA | 0x0f46 → 0x01ae | status `P` (0x01d1) | 0 → `Failed!` | `P`→`G`, ceremony 1, 1 → `Success!` |
| 4 | Mani | OTDA | 0x0f4c → 0x01fa → CAST2 0x03c2 | status != `D` (0x03d0) | 0 → `Failed!` | HP = min(HP + rand30, maxHP) via K 0x3f14, **returns 1 even at full HP** (0x03f6), ceremony 1 |
| 27 | Vas Mani | OTDA | 0x103a → 0x08ac | status != `D`; in the arena also `[0x58a3] != 0` (0x08d2-0x08f4) | 0 → `Failed!` | HP = maxHP, ceremony 5, 1 → `Success!` |
| 42 | In Mani Corp | OTD | 0x10ec → CAST2 0x05e0(idx, 0) | status `D` | 0, silent in the routine → tail `Failed!` | MANI-FINAL §2.2-2.4 |

Cancel (picker -1): every routine returns -1 before touching the target (0x0126, 0x01bf, 0x020a,
0x08bd; CAST2 0x05e0 returns -1 for idx < 0). The tail is silent, and **the charge and MP are already
spent** (§1). The world turn is spent; in the arena the actor's turn ends. CONFIRMED.

### 3.2 `Direction-` (one-step getdir)

| # | spell | where | code | effect on the adjacent cell | Pass |
|---|---|---|---|---|---|
| 5 | An Ylem | T, A | 0x0230 | tile 0x5b, 0x90-0x93, 0x9d, 0xa5-0xa6, 0xa8-0xa9, 0xad-0xaf → 0x44 + `POOF!` (0x459a), res -1; else 0 → `Failed!` | -1, silent |
| 6 | An Sanct | O, T, A (dungeon uses facing) | 0x02d2 / 0x0398 | 0xb9/0xbb → dec (unlocked) → 1; else a chest object (0x5c5a, tile 1) at the cell → trap bit cleared → 1; else 0 | -1, silent |
| 8 | Rel Hur | O | 0x0f64 → CAST2 0x040a(dir, 0) | wind: N→1, S→2, E→3, W→4 via K 0x2e96, ceremony 2 | **dir 0 with flag 0 → nothing at all** (0x040d-0x0417): no wind write, no ceremony |
| 17 | In Por | O (arena: no prompt) | 0x0680 | farthest grass along the line | -1, silent |
| 18 | An Grav | A (dungeon uses facing) | CAST2 0x0866 | a field object (0x5c5a, tile 0xe8-0xeb) **at the adjacent cell** deleted (K 0x3a74) → 1 | -1, silent |
| 25 | An Ex Por | T, A | 0x0846 | seals a door | (not traced) |
| 26 | In Ex Por | T, A | CAST2 0x0768 (0x076e) | magic-door worker | -1 → silent tail |
| 28, 40, 44, 45 | In Zu, In Nox Hur, In Vas Grav Corp, In Flam Hur | A | 0x1f60 `call 0x0306` at **0x1f8a** | the 21-ray fan of 0x1c36 in that direction: origin at the edge midpoint (0x1c6a-0x1cec), slope accumulator (0x1e7d-0x1ef4), LOS 0x6a14 on odd pixel rows, cap 63 | see §6.1 |

### 3.3 `Aim! ` crosshair (COMSUBS 0x0c52 → 0x0a68, range 15)

| # | spell | weapon | note |
|---|---|---|---|
| 1 | Grav Por | 0x30 | arena only |
| 13 | Vas Flam | 0x31 | arena only |
| 37 | Xen Corp | 0x32 | arena only |
| 14, 15, 16, 20 | In Flam / Nox / Zu / Sanct Grav (arena) | 0x35 / 0x33 / 0x34 / 0x36 (DS 0x4592) | CAST 0x00ec; the dungeon branch uses facing and has no prompt |

Cancel → `jmp 0x0bf0`: silent, and the tail is silent (these arms do not touch res). The spell is
spent and the turn ends. The crosshair cannot be confirmed on the actor's own cell. CONFIRMED.

### 3.4 `Creature: ` crosshair (COMSUBS 0x0504, range 15)

| # | spell | code | validity (in order) | invalid → | success |
|---|---|---|---|---|---|
| 34 | An Xen Ex (charm) | 0x09a0 | arena tile at the cell (K 0x4402 → 0xad14 + y·32 + x) with `& 0xfe` != 0x84 (0x09dc-0x09e2); a combatant at the cell (COMSUBS 0x0748: party/enemy, not gone); **not immune** — CAST 0x0000 type ∈ {0x0e Blackthorn, 0x0f Lord British, 0x2f Shadowlord} (0x09fc); **hostile** — K 0x5646 (an enemy not already charmed, or a charmed party member) (0x0a04-0x0a0c); **fails the saving** COMSUBS 0x0000 (one rand30) | res 0 → `Failed!` + glide | `xor flag,1`; a party member's status is set to `G`; `<name> charmed!` (0x45c6); res -1 → silent tail |
| 35 | Rel Xen Bet (polymorph) | 0x0a5c | a combatant at the cell (**any side**); not immune; fails the saving | 0 → `Failed!` | the record is removed (COMBAT 0x1236) and a rat (type 0x14) spawned at the cell (K 0x6506); 1 → `Success!` |
| 38 | In Quas Xen (clone) | 0x0b28 | a combatant at the cell (rest not traced) | 0 → `Failed!` | — |

Cancel → -1, silent, spent, the turn ends. CONFIRMED (in-place validity of 38 is UNKNOWN past the
lookup).

### 3.5 `To phase:` — Vas Rel Por (46), O/T/D

No location test besides the ship. **It prompts in the dungeon too** (CAST 0x0cf0-0x0d1d). What
K 0x47f4 does underground is UNKNOWN.

### 3.6 No target prompt

In Lor, An Xen Corp (7, 0x043e), In Wis, Kal Xen, In Xen Mani, Vas Lor, In Por **in the arena**
(0x05f3: random cell, `[0x58a1] & 2` gate, `Success!` / `Failed!`), In Sanct, Uus Por, Des Por,
Wis Quas (23, 0x074c), In Bet Xen, Rel Tym, In Vas Por Ylem, Quas An Wis, In An, Wis An Ylem,
Sanct Lor, In Quas Wis, In Quas Corp, Kal Xen Corp, An Tym. Dungeon field and An Grav / An Sanct use
facing. CONFIRMED by scanning each arm's calls for CAST2 0x0306, COMSUBS 0x0504 and CAST2 0x009e.

---------------------------------------------------------------------------------------------------

## 4. Native target model (as built)

- **Core seam** `cast_target_prompt` (`native/core/src/magic.cpp:98-126`).
  - Arena: `CombatReticle` whenever `target_type` contains `MapPosition`/`MapUnit` or equals
    `direction` (102-108).
  - World: keyed on the effect, `WorldDirection` only for Seal / Disarm / Blink / Unlock (111-116), and
    `WorldPhase` for Gate.
  - Dungeon: always `None` (109-110).
- **Device** `AlphaRuntime::cast_selected_spell` (`native/targets/tdeck/main/alpha_runtime.cpp:1762-1835`).
  This code is shared by the T-Deck and Windows (`native/targets/windows/CMakeLists.txt:156`).
  - `selectedCombatPlayer` → `On who:` picker **before any Cast is dispatched** (1790-1793).
  - `castingCombatPlayer` → `member = actor`, no prompt (1795-1797; this covers **Rel Xen Bet**).
  - Reticle "Spell aim", `Aim! ` echoed in Original mode (1805-1812).
  - World getdir `Direction-` (1813-1822).
  - Phase (1823-1829).
- **Reticle rules** (`native/core/src/ui_session.cpp`).
  - The cursor starts on the actor (alpha_runtime 1808).
  - A spell reticle has no range limit; only CombatAttack is range-checked (1166-1172).
  - Confirm on the own cell is blocked only for CombatAttack on the T-Deck (1233). Original mode blocks
    it for both (1034).
  - Cancel → `cancel_target` Cast (1115), plus `CombatYield` in Original (1119-1121).
- **Core effects** (`native/core/src/combat_magic.inc`).
  - Line: `spray()` collapses the aim to `sign(dx), sign(dy)` with **vertical priority** (143-146, 156).
    It returns with no effect if the aim is the own cell.
  - Charm / Polymorph / Illusion: the occupant at the aim, or **`nearest(a)` (nearest enemy) when aim is
    null** (245). An empty cell or self → `Nothing!` (246-249). Saving only (270). Charm **toggles**
    (273).
  - Dispel: the aimed cell, any distance (311-321).
  - Blink / Repel / Reveal ignore the aim (298-310, 214-239).
  - Poof / Disarm / Seal / Unlock have **no arena branch** (absent from `cast_effect`).
- **`combat_cast`** (`native/core/src/combat.cpp:1089-1145`).
  - `cast_spell` gates and spends first (1105-1110).
  - `cancel_aim` for Attack / Field / Line / Illusion / Dispel → `return` **without `advance()`**
    (1113-1117).
  - Heal with `member < 0` → `return` without `advance()` (1119-1120).
  - Field → Attack (1138-1139).

---------------------------------------------------------------------------------------------------

## 5. Candidate D — comparison and classification

(a) = MECHANICAL, (b) = PRESENTATION ONLY.

| spell(s) | binary | native | class |
|---|---|---|---|
| 28, 40, 44, 45 line spells | `Direction-` one-step getdir; four fans; Pass → see §6.1 | reticle on any cell, reduced to a direction (vertical wins on a diagonal); own cell → paid no-op | **(b)** for the reachable outcomes (the same four fans, byte-equivalent geometry: origin, slope, perpendicular sign, LOS row parity all match 0x1c36). Caveat: a diagonal aim silently becomes N/S. CONFIRMED |
| 34 An Xen Ex | `Creature: ` crosshair; hostile + not immune + tile check + saving; empty → `Failed!` | reticle (`Aim! ` in Original); any occupant, toggled; no immunity; no hostility test; empty → `Nothing!`; **cancel → nearest enemy charmed and the turn ends** (cancel_aim not honoured for Charm, combat.cpp:1113) | **(a)**: validity (allies, party, Shadowlord/LB/Blackthorn charmable; an ally can be un-charmed), cancel auto-target. **(b)** for the prompt word and `Nothing!`/`Failed!`. CONFIRMED |
| 35 Rel Xen Bet | `Creature: ` crosshair, any side, immunity | no prompt; nearest enemy | **(a)**. CONFIRMED |
| 38 In Quas Xen | `Creature: ` crosshair | reticle, `Aim! `; enemy only | (b) prompt word; validity (a)-PROBABLE |
| 8 Rel Hur (world) | `Direction-`; chosen wind; Pass = nothing | **no prompt; wind always 0 = "Calm"** (magic.cpp:186-191, `wind_arrow` has no producer; world_magic.cpp:45 passes 0) | **(a)**. CONFIRMED |
| 5 An Ylem (town) | `Direction-`, `POOF!` / `Failed!` | no prompt, no effect, charge and MP spent (magic.cpp:73-80 claims "no world effect", which is false for towns) | **(a)**. CONFIRMED |
| 5, 6, 25, 26 in the arena | `Direction-` on the adjacent cell | reticle any cell; **no arena effect at all** | **(a)** (An Ylem certainly; doors and chests in arenas are rare). CONFIRMED |
| 18 An Grav (arena) | `Direction-`, adjacent field only | reticle; removes the field at **any** cell | **(a)** range. CONFIRMED |
| 1, 13, 37, arena 14/15/16/20 | `Aim! ` crosshair, range 15 (whole arena) | reticle, unlimited (equivalent) | **(b)**: start cell (last target vs actor), diagonals, T-Deck own-cell confirm (a paid no-op), the cancel turn (T-Deck keeps it: the declared handheld contract, WINDOWS_PORT_PLAN D24). Field placement itself is a separate known approximation |
| 7 An Xen Corp, 23 Wis Quas, 17 In Por (arena) | no prompt | an extra reticle whose aim is ignored | **(b)** on the T-Deck. In Original mode, see §6.4 |
| 46 Vas Rel Por | `To phase:` in O/T/**D** | O/T only (magic.cpp:109-110) | (a)-PROBABLE in the dungeon (the outcome there is UNKNOWN) |
| heal family | `On who:` after the gates | `On who:` **before** the gates (alpha_runtime 1790) | see §7-8 |

## 6. Specific settlements

### 6.1 Line spells: four directions or any cell? **Four directions in both. Presentation, not range.**

- The binary takes only a direction (CAST 0x1f8a → CAST2 0x0306; 0x1c36 switches on 1..4 at
  0x1c6a / 0x1e80 / 0x1ecb). CONFIRMED.
- Native's reticle can point anywhere, but `spray()` reduces it to the same four directions
  (combat_magic.inc:143-157). The cells hit for a given direction match the binary's ray walk, so it is
  **not** a mechanical divergence in range or cells.
- What is native's own: the reticle→direction rule (vertical priority), the own-cell no-op on the
  T-Deck, and the misleading "aim at a cell" affordance.
- **Pass (Space) at the binary's line-spell `Direction-`** returns 0. 0x1c36 then applies no origin
  shift and no axis or perpendicular step. The 21 rays never move. Their pixel (x·16+8, y·16+8) is on an
  even row inside the clip, so 0x1bb0 always returns 1. No ray is ever marked done (0x1e68), and the pass
  loop 0x1cff-0x1f4e cannot end. **PROBABLE original hang** (static; not run). Native must not reproduce
  it. Treat Pass as "spent, no effect, the turn ends" and record it.

### 6.2 Charm: crosshair with a creature-validity check? **Yes.**

`Creature: ` + COMSUBS 0x0504, then the checks in §3.4. **Hostility (K 0x5646) and immunity
(CAST 0x0000) are both checked before the saving.** CONFIRMED.

### 6.3 Rel Hur's direction

The spell uses `Direction-` (CAST2 0x0306), then CAST2 0x040a(dir, 0). The scroll uses the same getdir
(CAST 0x1229) with flag 1, where Pass writes calm and plays ceremony 1. **The spell with Pass changes
nothing.** CONFIRMED. Native's spell sets calm unconditionally. Native's **scroll** with Pass leaves the
wind unchanged (world_magic.cpp:30), where the binary calms it. That is a minor (a), adjacent.

### 6.4 Original-mode double turn advance (PROBABLE, needs a RED test)

`ui_session.cpp:1115-1121` sends `CombatYield` after **every** cancelled Cast reticle.
`combat_cast` skips `advance()` only for Attack / Field / Line / Illusion / Dispel (combat.cpp:1113).
Charm / Repel / Reveal / Blink / Poof / Disarm / Seal / Unlock run their effect (Charm on the nearest
enemy) **and** advance. The Yield then advances again (combat.cpp:1438-1441); when the next actor is a
party member, its turn is skipped.

---------------------------------------------------------------------------------------------------

## 7. Candidate F — the binary sequence for cancelled targets

**Cast, world and town** (CONFIRMED):

1. `Cast...` (K 0xa142) → `Player: ` (K 0x4988) → `Spell name:` → name.
2. Location gate: Absorbed / Not here, nothing spent.
3. `None mixed!`, nothing spent.
4. **Charge −1** (0x0ec8).
5. MP < circle → `M.P. too low!` + `Failed!`, MP kept.
6. **MP −= circle** (0x0ef8).
7. Level < circle → `Failed!`.
8. Arm → `On who: ` → ESC → `None!` on the row.
9. Arm returns -1 → silent tail → returns 1 → **one world turn**.

**The arena** is identical (caster = `g_cmb_actor`, the gate uses bit 1), and **the actor's turn ends**
(COMBAT 0x0923). **The dungeon** has the same spend; its turn is UNKNOWN.

This applies to all five `On who:` spells (An Zu, An Nox, Mani, Vas Mani, In Mani Corp). **No heal
spell refunds anything.** Gates that refuse never show `On who:`.

**(U)se potion** (CAST 0x135a), CONFIRMED:

1. **Potion −1 at 0x136a**, then `Potion` (DS 0x4706).
2. Outside the arena: `On who:` (0x138e). ESC → -1 → `jmp 0x1533` returns AX = -1 → (U)se 0x1819
   `[bp-0xa] = -1` → epilogue 0x1b8a prints nothing.
3. The kernel `U` arm leaves `[bp-2] = 1` (K 0x340c-0x3416) → **one world turn**.
4. In the arena there is no picker: the actor drinks (0x137c).

**In Mani Corp scroll** (CAST 0x11de / 0x12d8), CONFIRMED:

1. **Scroll −1 at 0x11ec** before the jump table → `Scroll` → `Resurrection!` → `On who:`.
2. ESC → `None!` → CAST2 0x05e0 returns -1 → reader returns -1 → no `Failed!` → one world turn.
3. In the arena: `Not here!` with the scroll still consumed.

This settles §3.6's "?" in the fidelity doc: **a cancelled resurrection scroll is spent.**

## 8. Native behaviour on these paths, and the ledger reconciliation

| path | binary | native core, if called with member = -1 | device T-Deck / Enhanced | Windows Original |
|---|---|---|---|---|
| world heal, `On who:` ESC | charge + MP spent, `On who: None!`, 1 turn | spends both, silent, no turn (world_magic.cpp:64; commands.cpp:946-951 charges a turn only for Blink) | **nothing dispatched: charge and MP kept, no turn, no text** (alpha_runtime 1790-1793; cancel 1445 drops `pending_combat_spell_`) | `On who: None!` printed (1430); same refund |
| arena heal, ESC | spent, the turn ends | spends; `return` without `advance` (combat.cpp:1119) | refund; the turn is kept | same as the T-Deck |
| dungeon heal, ESC | spent (turn UNKNOWN) | spends + one dungeon tick (dungeon_orchestration.cpp:178-182) | refund | refund |
| gate would refuse | no `On who:`; Not here / None mixed / MP / level as §1 | — | picker shown first; the gate message comes only after a pick; **cancel hides the refusal and refunds even the charge the MP gate would have burnt** | same |
| potion, world, ESC | potion spent, 1 turn | `consume_potion` (world_magic.cpp:25) | **refund (D-51)** | spent (the DOS-only dispatch, alpha_runtime 1437-1443); no turn (§5 shared rule) |
| scroll 6, world, ESC | spent, `Resurrection!` before `On who:` | spends (world_magic.cpp:27) | refund (alpha_runtime 1520 opens UseTarget for item 6; cancel drops it) | **refund** (1440 spends only 8..15) |

**Where the refund lives: the device application, not the core.** CONFIRMED. The core spends whenever it
is called (`cast_spell` magic.cpp:168-174, before any target use). The refund is AlphaRuntime's
**call order**: it opens the picker before dispatching any Cast, and on cancel it discards the parked
spell. The rule is the same for all five spells and in every context. Only the visible text differs:
Original prints `On who: None!`.

**Side finding (CONFIRMED by reading; needs a RED test).** `cast_selected_spell` resets
`pending_caster_` (1779) before opening the `On who:` picker. The accept arm (1582) then rebuilds the
caster as `current_combat_actor(...) ?: active_member(game_)`. **Outside the arena, a heal spell is
therefore paid for and level-gated by the active member, or member 0 when none is active, not by the
member chosen at `Player:`.** In the binary, `[bp-4]` from 0x0dd5 pays at 0x0ef8 and is level-checked at
0x0f01.

**Ledger reconciliation**

- **D-51** (`docs/hardware/ALPHA2_PRESERVATION_LEDGER.md:201`): "Cancelling a potion's target picker
  refunds the potion; the reference consumes it at selection". Classed native divergence,
  player-favourable, open (H-45 hardware FAIL; listed under "open, non-blocking" in the A4-CLOSE1
  totals, :489).
  - It is **not** a declared deliberate divergence: no A-/E- row covers refunds.
  - "Stays on the T-Deck and in Enhanced" (fidelity §3.6) describes the current state; it is not a
    recorded decision.
- **D-101** (`ledger:251`, `ALPHA4_UI.md:3762`), open. It bundles four things: the In Mani Corp
  ceremony, the world turn of (U)se/(C)ast, the "device refund", and the glide. Its refund half is
  **device-only**: the TS reference spends, because `castSpell` runs before `pickCastTarget`
  (`game/src/main.ts:4422` then `4445`; the cancel only appends `None!`, `game/src/ui/pickers.ts:199-209`).
- **Fidelity item 7** ("Native refunds a heal spell when `On who:` is cancelled (D-101) — core") is
  right about the behaviour and wrong about the layer and the scope.
  - Layer: AlphaRuntime, not the core.
  - Scope: all five spells plus scroll 6; the MP as well as the charge; and in the arena the actor's
    turn.
  - It also belongs with item 6 (prompts before gates), because the same pre-dispatch picker produces
    both.
- **Fidelity item 11** is CONFIRMED and is (a), not presentation: native's Rel Hur always calms the wind.
- **Fidelity item 3** is incomplete:
  - Rel Xen Bet asks **nothing** and auto-targets the nearest enemy.
  - An Xen Corp, Wis Quas and arena In Por get a reticle the binary does not have.
  - An Ylem, An Sanct, An Grav, An Ex Por and In Ex Por in the arena use `Direction-`, not a reticle.

## 9. Side findings and secondary-source errors

1. **Mani on a full-HP target prints `Success!`** in the binary (CAST2 0x03f6: res 1 for any non-`D`).
   Native (magic.cpp:229-231, `hp > old`) and TS (`main.ts:4447`, `applyMani(...) > 0`) print `Failed!`.
   (a)-text. CONFIRMED.
2. **An Zu success is silent** in the binary (0x0183 res = -1; the `[bp-2] = 0` at 0x0177 is a dead
   store). Native (world_magic.cpp:64, combat.cpp:1122) and TS print `Success!`. CONFIRMED.
3. Arena Vas Mani needs `[0x58a3] != 0` (0x08d2-0x08f4). Its meaning is UNKNOWN; neither port has it.
4. Heal ceremonies play inside the arms, on success only (index 1, or 5 for Vas Mani). Native plays the
   circle ceremony before every applied cast (world_magic.cpp:47). This is the D-101 family.
5. `game/src/main.ts:3305-3310` (combat An Grav comment) says CAST2 0x0306 "moves the cursor key by key
   without a range cap until confirm". **False**: it reads one key and steps once (0x034c-0x03b4).
   Native's any-cell An Grav reticle inherits this.
6. `game/src/core/magic/cast.ts:202-205` says the binary "distinguishes cancel (di<0) from Space = calm"
   for the Rel Hur spell. **False**: 0x0306 never returns < 0, and Space with flag 0 writes nothing.
7. `native/core/src/magic.cpp:73-80` says An Ylem "has NO world effect at all". **False for towns**
   (mask 0x05, CAST 0x0230). The An Grav half is correct.
8. Native world getdir cancel prints `Cancelled.` for Seal / Disarm / Unlock (world_magic.cpp:68, 84),
   after `Direction-Pass` in Original. The binary is silent. (b).
9. `fx-lineaoe-negate-derivation.md` §3a "getdir (prompt de dirección)" is right about the routine but
   does not name it. It is CAST2 0x0306 `Direction-`, not K 0x35EC.

## 10. Recommendations

### 10.1 Narrowest correct layer

**F (cancelled heal / scroll / potion).**

- **Device layer (AlphaRuntime), plus one small core addition.**
  1. Add a pure `cast_refusal(const GameState&, const CharacterState&, SpellId, CastContext)` in
     `magic.h` and factor `cast_spell` through it (magic.cpp:150-176). No behaviour change; `magic.txt`
     stays byte-identical.
  2. In `cast_selected_spell`, if a refusal (or arena absorb) is due, dispatch the Cast untargeted
     **instead of** opening any prompt. The core then prints the refusal and spends exactly what the
     binary spends. This also fixes item 6 for every prompt kind.
  3. On `On who:` cancel, dispatch the Cast with `member = -1`, mirroring the existing DOS potion path
     (1437-1443). In the arena, end the turn: either make `combat_cast` call `advance()` for
     `member < 0` (core, combat.cpp:1119) or send `CombatYield` as ui_session.cpp:1119 does. The core
     route is cleaner and is not pinned by `advanced-combat.txt` (its generator always passes a member).
  4. Keep the `Player:` caster across the picker (fixes the §8 side finding).
  5. Extend the Original-mode spend at 1440 to scroll 6.
- **T-Deck / Enhanced:** D-51 and D-101's refund are not deliberate. The user must either ratify them as
  an E-row (player-favourable handheld contract) or apply the same change everywhere. Do not leave the
  T-Deck by accident. The world-turn half is the §5 shared turn rule; keep it a separate, explicit
  decision.

**D (targeting).**

1. **Core seam** `cast_target_prompt` keyed on the binary arm, not `target_type`:
   - `CombatDirection` (one step): 5, 6, 18, 25, 26, 28, 40, 44, 45.
   - `CombatCreature` (`Creature: `, range 15): 34, 35, 38.
   - `CombatAim` (`Aim! `): 1, 13, 37, 14, 15, 16, 20.
   - `None` in the arena: 7, 17, 23.
   - Add `WorldDirection` for Rel Hur (outdoors) and An Ylem (town).
   - Update `batch5_test.cpp:150-166`, which pins the current values.
2. **Core effects, reference first** (memory: fix the reference at the pinned layer;
   `advanced-combat.txt` pins `Nothing!` and the null-aim charm/polymorph):
   - Charm / Polymorph validity (hostility, immunity, `Failed!`, no auto-nearest; cancel = nothing).
   - Rel Hur `wind_arrow` from the direction (W1 E2 N3 S4) with a Pass sentinel that writes nothing.
     `magic.txt` uses 0..5 only, so it is unaffected if the sentinel is new.
   - Arena Poof / Disarm / Seal / Unlock branches.
   - Town An Ylem.

**Can the T-Deck reticle stay with binary-equivalent mechanics?**

- **Yes for `CombatDirection`**, if the reticle can only occupy the four adjacent cells (or one trackball
  press commits a direction). The core already turns an adjacent aim into the binary's fan (lines) or
  cell (An Grav and the others). The fixtures are unchanged because they reduce to the same four
  directions.
- **Yes for `Aim!` / `Creature:`** (range 15 already covers the arena). Block own-cell confirm and keep
  "cancel = spent" (the turn per the declared contract).
- Only the label (`Spell aim` vs `Aim!` / `Creature:` / `Direction-`) and the start cell remain
  presentation.

### 10.2 RED-first test plan

Each item names its home, the assertions, and whether it is RED today.

**F1** — `native/targets/tdeck/host_tests/win_dos_runtime_test.cpp`, Original, outdoors. Mani, mixed 2,
caster MP 10, level 1; `Player:` picks member 1 while member 0 is active. Pick `On who:`, then ESC.

- Expect `spell_quantities[4] == 1`, member 1 MP 9, member 0 MP unchanged.
- Expect the transcript to end `On who: None!` with no `Success!` / `Failed!`.
- Expect `rng` draw count unchanged.
- RED today (the charge and MP are kept).
- The `turns_since_start` +1 assertion is a separate test, gated on the §5 decision.

**F2** — Same harness, arena. The same cast, then ESC.

- Expect the charge and MP spent, `current_combat_actor` changed (turn ended), and `combat.rng` untouched.
- RED.

**F3** — Gates before the prompt.

- MP 0: no PartySelection is ever opened, transcript `M.P. too low!` / `Failed!`, charge −1, MP 0.
- In Mani Corp in the arena: `Not here!`, nothing spent, no picker.
- Level 1 with Vas Mani: `Failed!`, charge and MP spent, no picker.
- All RED.

**F4** — Wrong-caster guard.

- After picking member 1 at `Player:` and a member at `On who:`, member 1 pays the MP and the level gate
  reads member 1. RED.

**F5** — Scroll 6, Original, ESC.

- Expect `scroll_quantities[6]` −1, and transcript order `Scroll` / `Resurrection!` / `On who: None!`
  with no `Failed!`.
- RED (refund, and the order).

**F6** — Guards that must stay green.

- A picked target gives the existing results.
- Potion Original-mode ESC still spends.
- Dungeon heal cancel spends and ticks once.
- `magic.txt`, `advanced-combat.txt` and `gameplay_parity` stay byte-identical.

**F7** — Mutations to kill:

- refund the charge only; refund MP only;
- print `Failed!` on cancel; keep the arena turn;
- open the picker before a refusing gate;
- use the active member as caster.

**D1** — `native/core/tests/batch5_test.cpp`.

- `cast_target_prompt(RelHur, world) == WorldDirection`.
- `cast_target_prompt(AnYlem, town) == WorldDirection`.
- `cast_target_prompt(InZu / AnGrav / AnSanct, combat)` == the new `CombatDirection`.
- `cast_target_prompt(RelXenBet, combat)` == `CombatCreature`.
- `cast_target_prompt(AnXenCorp / WisQuas / InPor, combat) == None`.
- All RED. The old pins at 150-166 are replaced.

**D2** — Rel Hur, core plus world_magic.

- North → `turn.wind == 1`; West → 4.
- Pass → wind unchanged, no ceremony event, charge and MP spent.
- RED (today the wind is always 0).

**D3** — Charm, a new core test plus a TS test first.

- An allied (charmed) enemy → `Failed!`, `charmed` stays true, zero saving draws.
- A party member → `Failed!`.
- Types 14 / 15 / 47 → `Failed!`, zero draws.
- An empty cell → `Failed!`, not `Nothing!`.
- A null aim (cancel) → no actor changes, the turn ends.
- A hostile enemy that fails the saving → toggled, `<name> charmed!`, no `Success!`.
- RED except the last.

**D4** — Rel Xen Bet.

- An explicitly aimed non-nearest enemy (and a party member) becomes the rat; an immune type → `Failed!`.
- RED.

**D5** — Line spells.

- For each of the four adjacent aims, the hit set equals the current spray (green guard: proves the
  reticle constraint is outcome-neutral).
- The Original UI prompt is `Direction-` with the word appended; Space → `Pass`, spent, the turn ends,
  no hang.
- The T-Deck reticle cannot leave the adjacent ring.
- RED for the UI parts.

**D6** — An Grav, arena.

- A field two cells away cannot be targeted (the UI only offers adjacent cells); an adjacent field →
  removed, `Success!`.
- RED.

**D7** — Original-mode double advance (§6.4).

- Cancel the An Xen Corp reticle with the next actor a party member: that member still gets its turn.
- PROBABLE RED.

**D8** — Resolution text, reference first (main.ts plus the check-gameplay replica, then native).

- Mani at full HP → `Success!`; An Zu success → no `Success!`.
- RED in both ports. The `advanced-combat-coverage.json` `text:Success!` count moves, so regenerate
  with `--check` control and a token diff.

## 11. Residual unknowns

- The dungeon turn for a cancelled cast or use.
- The arena Vas Mani gate `[0x58a3]`.
- In Quas Xen validity past COMSUBS 0x0748.
- The meaning of arena tile 0x84/0x85 in the charm test.
- K 0x47f4 for Vas Rel Por underground.
- The An Ex Por arena outcome.
- The §6.1 Pass hang needs a DOS witness.
- No item here was confirmed in a DOS run.
