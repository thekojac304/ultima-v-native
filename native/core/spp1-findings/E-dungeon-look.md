# SPP1 Candidate E: dungeon (L)ook

Read-only investigation. I re-disassembled everything in sections 1 to 4 with `re/tools/dis16.py`, read the overlay thunks with `re/tools/thunks.py`, and ran a scratch near-call census (`C:\Users\lawma\AppData\Local\Temp\spp1-E\census.py`; E8/E9 scan over ULTIMA.EXE (base 0) and every overlay at its fitted base). The census has a positive control: for `0x3178` it finds exactly DUNGEON `0x07a3`, MAINOUT `0x0c00` and TOWN `0x158f`, which matches `D86-FINAL.md` §2.5.

Notation:
- `K:0xXXXX` is a ULTIMA.EXE image offset (`--exe`).
- `DNGLOOK:0xXXXX`, `SJOG:0xXXXX` and `DUNGEON:0xXXXX` are overlay file offsets.
- `DS 0xXXXX` is a DATA.OVL string at file offset DS+0x10.

Overlay near calls use the fitted bases: DUNGEON 0x81D0, DNGLOOK/LOOKOBJ 0xA290, SJOG 0xBF80. Every resolved target was checked for a `55 8b ec` prologue or a thunk record. Everything here is static analysis. No DOS run exists.

---------------------------------------------------------------------------------------------------

## 0 Verdict (one paragraph)

**CONFIRMED.** Dungeon (L)ook is `DNGLOOK.OVL:0x0000`. The kernel dispatcher's `L` arm calls it only when `0x21 <= g_location <= 0x28`.

The command runs in this order:
1. It prints `Look...`.
2. It resolves the acting member with the kernel picker `0x4988` (`Player: `).
3. It applies a darkness gate.
4. It asks `Dir-` through the SAME prompt routine the dungeon (S)earch uses (SJOG `0x006c`: ↑ Ahead, ↓ Here, → Right, ← Left, Space Pass).
5. It describes the raw `g_dng_map` byte of the chosen cell.
6. If that cell is a fountain (`tile & 0xF0 == 0x50`), it asks `Will you drink?`, accepting only Y or N. On Y, the member chosen at step 2 gets the fountain's effect.

The kernel returns **1 for every path**: cancel, darkness, Pass, `No.` and drinking. Look therefore always consumes a dungeon world turn (`DUNGEON:0x0c76`) and is followed by the loop-top `advance_clock(1)`.

There is **no Drink command** in the binary. In a dungeon (and everywhere else) `D` is the kernel's `D-What?` arm, which returns 0 (no world turn). Look is the only way to drink from a dungeon fountain.

Native has no dungeon Look at all. On the T-Deck and Windows, `L` prints `What?`. The `D` shortcut drinks from the party's own cell for the active-or-first member and invents a `No fountain here.` message.

---------------------------------------------------------------------------------------------------

## 1 The handler chain

### 1.1 DUNGEON loop to the kernel dispatcher (CONFIRMED)

- `DUNGEON:0x0f36` reads the key. `0x0f42 call 0x06c4` hands it to the key handler, and `0x0f45 mov si,ax` stores the result. (This loop segment comes from `D86-FINAL.md` §2.3. I re-read only `0x06c4`-`0x07de`.)
- `DUNGEON:0x06c4` (`0x06cb [bp-2]=1`) branches on the key:
  - `0x06d3 cmp 0xb`
  - `0x06dd`/`0x06e5` for 1..4 (move)
  - `0x06ea` for 5
  - `0x07a8` for 0x0d / 0x13 / 0x16 / 0x2e
  - `0x07bc` for digits 0x30..0x39
- Anything else goes to `0x07a0 push key / 0x07a3 call 0xffffafa8`, which resolves to (0xafa8+0x81D0)&0xFFFF = **K:0x3178**.
- `0x07a6 jmp 0x771`, then `mov [bp-2],ax`, `0x070a mov ax,[bp-2]` and `ret 2`. The handler returns the kernel's value unchanged.
- `L` (0x4C) and `D` (0x44) are not in that list, so both reach the kernel.

### 1.2 Kernel dispatcher, `L` arm (CONFIRMED)

- `K:0x317e mov word [bp-2],1` is the default return value, and `K:0x31ee mov ax,[bp-2]` then `0x351c ... ret 2` returns it.
- `K:0x3480 sub ax,0x46 / cmp 6 / jmp cs:[bx+0x3490]`. The jump table at K:0x3490 decodes to F→0x3266, G→0x3274, H→0x3288, I→0x32cc, J→0x32da, K→0x32e8, **L→0x3310**.
- `K:0x3310` does the following:
  1. `print DS 0xa1a8` (`Look`).
  2. `0x3317 cmp [g_location 0x5893],0x20 / jbe 0x3332` and `0x331e cmp 0x29 / jae 0x3332` restrict the dungeon arm to 0x21..0x28.
  3. `0x3325 print DS 0xa1ae` (`...\n`).
  4. `0x332c call 0x7f32`.
  5. `0x332f jmp 0x31ee` returns **`[bp-2]` = 1**. The arm never writes `[bp-2]`.
- Outside dungeons the arm is `0x3332 putchar('-')` then `0x3339 call 0x7f26`. Thunk 0x7f26 goes to overlay 9 (LOOKOBJ) at 0xac2c = LOOKOBJ:0x099c `cmd_look`.

### 1.3 The thunk resolves to DNGLOOK:0x0000 (CONFIRMED)

- Thunk `K:0x7f32` is `9a ec02 2e07 / 0a 00 / ea 90a2 0000`: it loads overlay **10** and jumps to **0xa290**.
- The overlay filename table at ULTIMA.EXE file offset 0x8108 orders the overlays ULTIMA.EXE=0, TOWN=1, MAINOUT=2, DUNGEON=3, INTRO, FLAMES, NPC, COMBAT, BLCKTHRN, LOOKOBJ=9, **DNGLOOK=10**, OUTSUBS, SHOPPES, ENDGAME, **SJOG=14**, CMDS=15, ...
- 0xa290 minus the DNGLOOK base 0xA290 is **DNGLOOK:0x0000**.
- Census: `0x7f32` has exactly one near caller, **K:0x332c**.

### 1.4 `DNGLOOK:0x0000`, the whole body (`0x0000`-`0x0282`, `ret` with no arguments) (CONFIRMED)

| offset | instruction(s) | meaning |
|---|---|---|
| 0x0007 | `call 0xffffa6f8` | (0xa6f8+0xA290)&0xFFFF = **K:0x4988** `resolve_command_char` |
| 0x000a | `mov [bp-0xc],ax` | **the acting member; this is the drinker** |
| 0x000d-0x0010 | `inc ax / jne / jmp 0x27e` | -1 means return immediately |
| 0x0013-0x0024 | `cmp [0x58a7],0 / jne` · `cmp [0x58a6],0 / jne` · else `ax=DS 0x752e`, `jmp 0xdf` | torch minutes == 0 AND light-spell minutes == 0 prints `You see:\ndarkness.\n` and returns. **This happens AFTER `Player:` and BEFORE `Dir-`.** |
| 0x0028-0x0035 | `push [g_dng_facing 0x6603] / call 0xffffdcae / or ax,ax / jne / jmp 0x27e` | (0xdcae+0xA290)&0xFFFF = thunk **K:0x7f3e**, which loads overlay 14 at 0xbfec = **SJOG:0x006c** (`Dir-`); a return of 0 means return |
| 0x0038-0x0066 | `ax=[g_cmb_scratch_x 0x5876]`, `[bp-8]=[g_cmb_scratch_y 0x5878]`, `si=(y&7)<<3 + (x&7)`, `bx=g_floor<<6`, `al=[bx+si+0x595a]` | the **RAW** `g_dng_map` byte of the target cell, toroidally wrapped (`&7`). It does NOT go through `dng_get_tile` (DUNGEON:0x10dc), so bit 0x08 is kept. |
| 0x0069-0x006d | `print DS 0x7542` | `You see:\n` |
| 0x0070-0x0076 | `cmp [bp-0xa],0x61 / jne / mov [bp-0xa],0` | **exact** 0x61 (hidden pit fall) is treated as 0x00 |
| 0x007b-0x00b5 | `(t&0xf0)==0x80`: exact 0x80, 0x81, 0x82, 0x83, else default | field strings (§1.7), then return |
| 0x00b8-0x00ff | `(t&0xf0)==0xc0`: `[g_dng_wall_variant 0x6604]&0xf` ==1, ==2, else `push 1 / push 0xff / call 0x7e02` (K:0x2092 `rand_range(1,255)`), `cmp ax,0xff` | special-wall strings, then return |
| 0x0102-0x01e6 | word switch on `t&0xf0` | class strings (§1.7) |
| 0x012f-0x0142 | `(t&0xf0)==0x50` gives `print DS 0x7700` and `jmp 0x1f0`, else `jmp 0x27e` | the fountain question |
| 0x01ea-0x01f8 | `call 0xffff83dc` (K:0x266c `getkey_with_redraw`); loop until `'Y'` (0x59) or `'N'` (0x4e) | **only Y/N leave the loop.** ESC, Space and RETURN are re-read. K:0x266c uppercases a..z through K:0x2032, so `y`/`n` work. |
| 0x01fa-0x0203 | `'N'` prints DS 0x7712 and returns | `No.\n` |
| 0x0206-0x020a | `'Y'` prints DS 0x7718 | `Yes.  Gulp!\n` (two spaces) |
| 0x020d-0x021f | **exact** word compare of the full tile: 0x50, 0x51, 0x52, else default | effect switch |
| 0x0222-0x0235 | `print DS 0x7726`; `[[bp-0xc]*32+0x55b3]='G'` | `Cured!\n`; status (record +0x0B) = 'G', unconditionally |
| 0x0238-0x024e | `print DS 0x772e`; `[m*32+0x55b8] = [m*32+0x55ba]` | `Healed!\n`; HP (+0x10) = max HP (+0x12) |
| 0x0250-0x0263 | `print DS 0x7738`; `[m*32+0x55b3]='P'` | `Poisoned!\n`; status = 'P', unconditionally |
| 0x0266-0x027b | `print DS 0x7744`; `push [bp-0xc]`; `rand_range(0,7)` (K:0x2092); `call 0xffff87c2` = **K:0x2a52** `apply_damage(member, amount)` | `Bad taste.\n`, then 0..7 damage to the SAME member |
| 0x027e-0x0282 | `pop si / mov sp,bp / pop bp / ret` | |

The body contains no other calls and touches no other globals. It never reads the actor or monster tables, never moves the party, never changes floor or location, and never writes `g_dng_map`.

### 1.5 The member picker `K:0x4988`, as Look experiences it (CONFIRMED)

(Read `0x4988`-`0x4a83` and the picker `0x2d7a`-`0x2e8b`. This agrees with `openu5/command_char.h`.)

The picker resolves the member in this order:

1. **Active member set (`[0x587b] != 0xff`)**: it is returned silently, with **no status check** (`0x49b2`-`0x49bc`).
2. **One member with status 'G' or 'P'**: that member is returned silently.
3. **No member with status 'G' or 'P'**: it prints `None!\n` (DS 0xa3da, epilogue `0x4a65`) and returns -1 (`0x49c8`-`0x49fe`).
4. **Two or more eligible members**: it does the following.
   - It prints `Player: ` (DS 0xa3c4) and calls `0x2e8e` → `0x2d7a`. That opens the `Select:` band (DS 0x5554).
   - Digits 1..party_size move the highlight. ↑/← and ↓/→ step the cursor. RETURN or Space choose, **ESC returns -1**, and keys above `'7'` are re-read.
   - An eligible pick prints the name (`0x4a30`-`0x4a3a`).
   - An ineligible pick prints `Disabled!\n\n` (DS 0xa3ce) and asks again.
   - **ESC prints `None!\n`** (`0x4a12` → `0x4a5f`-`0x4a69`).

**Consequence (CONFIRMED by the effect code).** The drinker is the picked member, not "the active member, else member 0". An asleep active member can be returned through branch 1. A cure fountain then sets them to 'G'.

### 1.6 The `Dir-` prompt `SJOG:0x006c` (CONFIRMED)

- `0x0072 print DS 0x84e6` (`Dir-`).
- The loop `0x0094 call 0x66ec` (K:0x266c) breaks only on key codes **1, 2, 3, 4 or 0x20** (`0x007c`-`0x009c`). Other keys, **ESC included, are ignored**.
- The arrow codes are confirmed by the extended-scancode table in DATA.OVL at DS 0x540e: `4b 4d 48 50` → DS 0x5416 `01 02 03 04`. So Left=1, Right=2, Up=3, Down=4. The numpad map at K:0x26fe agrees: '8'→3, '2'→4, '4'→1, '6'→2.

| key | prints | target written to g_cmb_scratch | return |
|---|---|---|---|
| ↑ (3) | `Ahead\n` (DS 0x84f2) | party + delta(facing), via `SJOG:0x002a` | 1 |
| ↓ (4) | `Here\n` (DS 0x84fa) | party's own cell (`0x00eb`-`0x00f6`: `[g_party_x 0x5896]`, `[g_party_y 0x5897]`) | 1 |
| → (2) | `Right\n` (DS 0x8500) | delta((facing+1)%4) | 1 |
| ← (1) | `Left\n` (DS 0x8508) | delta((facing+3)%4) | 1 |
| Space | `Pass\n` (DS 0x84ec) | none | **0** |

- `SJOG:0x002a` applies the delta: dir 0 y-1, 1 x+1, 2 y+1, 3 x-1. It does not wrap; DNGLOOK applies `&7`.
- Census: `SJOG:0x006c` has exactly two callers, **SJOG:0x0678** (dungeon Search) and **DNGLOOK:0x002e** (through the thunk). So Look's `Dir-` is byte-for-byte the dungeon Search's `Dir-`.
- In both commands the member pick comes first: SJOG `0x064e` calls 0x4988 before `0x0678`, and DNGLOOK `0x0007` before `0x002e`.

### 1.7 What Look prints for each cell

All entries are CONFIRMED. Strings are DATA.OVL bytes; native prunes the trailing `\n` by convention. Every lit line is preceded by `You see:\n` (DS 0x7542).

| raw tile | branch | text (DS) |
|---|---|---|
| 0x61 exactly | remap to 0x00 (`0x0070`) | `a passage.\n` (0x7618). The hidden pit fall is not revealed; 0x69 is NOT remapped. |
| 0x00-0x0F | class 0x0 | `a passage.\n` (0x7618) |
| 0x10-0x1F | 0x1 | `an up ladder.\n` (0x7624) |
| 0x20-0x2F | 0x2 | `a down ladder.\n` (0x7634) |
| 0x30-0x3F | 0x3 | `a ladder.\n` (0x7644) |
| 0x40-0x4F | 0x4 | `a wooden chest.\n` (0x7650) |
| 0x50-0x5F | 0x5 | `a fountain.\n` (0x7662), then `Will you drink?\n` (0x7700) |
| 0x60, 0x62-0x6F | 0x6 | `a pit.\n` (0x7670), including bomb traps 0x62/0x6A and pit falls with bit 0x08 (0x69) |
| 0x70-0x7F | 0x7 | `an open chest.\n` (0x7678) |
| 0x80 / 0x81 / 0x82 / 0x83 | field, exact | `A sleep field.\n` (0x754c) / `A poison gas field.\n` (0x755c) / `A wall of fire.\n` (0x7572) / `An electric field.\n` (0x7584) |
| 0x84-0x8F | field default | `An energy field.\n` (0x7598). The bit-0x08 copies of 0x80-0x83 land here too. |
| 0x90-0x9F | 0x9 | `nothing of note.\n` (0x769a) |
| 0xA0-0xAF | 0xA | `a heavy door.\n` (0x76ac) |
| 0xB0-0xBF | 0xB | `a wall.\n` (0x76bc) |
| 0xC0-0xCF | special wall | variant 1: `a dripping stalactite.\n` (0x75aa). Variant 2: `a caved in passage.\n` (0x75c2). Any other variant: **`rand_range(1,255)`**; 255 gives `an unfortunate software pirate.\n` (0x75d8), else `a less fortunate adventurer.\n` (0x75fa). |
| 0xD0-0xDF | 0xD (secret door) | `a wall.\n` (0x76d6) |
| 0xE0-0xEF | 0xE | `a heavy door.\n` (0x76e0) |
| 0xF0-0xFF | 0xF (room) | `a heavy door.\n` (0x76f0) |
| (darkness) | gate at 0x0013 | `You see:\ndarkness.\n` (0x752e), one string with no separate header |

- Unreachable string: `SPEC WALL ERR.\n` (0x76c6, class 0xC in the word switch; 0xC0 is caught first). `An energy field.\n` at 0x7688 (class 0x8) is likewise unreachable.
- `g_dng_wall_variant` is set at DUNGEON `0x0e7b`-`0x0ec4`: location-0x20 ∈ {1,4,5} gives 3 (Deceit, Wrong, Covetous); {6,7} gives 2 (Shame, Hythloth); otherwise 1. Native already has this as `dungeon_wall_variant()` (`dungeon_view.cpp:38`).
- The task asked about monsters, altars and orbs. There is **no** such branch. Look never reads the wanderer or actor table, so a monster ahead is described by its floor tile (CONFIRMED by absence over the whole body). The corridor grid has no altar or orb class. Rooms are COMBAT arenas, where Look is refused. That refusal comes from the native combat refusal list `Look-Not here` (`ui_session.cpp`); I did NOT re-verify it in COMBAT.OVL, so it is PROBABLE.

### 1.8 Fountain drink: effects and side effects (CONFIRMED)

- The switch compares the **whole** tile byte: 0x50 Cure, 0x51 Heal, 0x52 Poison, and **everything else in 0x53-0x5F is `Bad taste.`**, including 0x58-0x5A, the bit-0x08 copies of the good fountains.
- Cure, Heal and Poison are direct record writes. They make no sound, no flash and no redraw call.
- `Bad taste.` calls `K:0x2a52(member, rand(0,7))`, which does the following:
  1. It flashes the member's roster row (`0x2a59`, `0x2a6e`).
  2. It plays the noise burst `K:0x223c(0x7d0,0x640,0xa)`. That burst uses its own generator `[0x545c]` (`0x2255`-`0x2265`), not `g_rng [0x5420]`, so it draws nothing from the game RNG.
  3. It subtracts the amount from HP (0 is possible).
  4. If HP <= 0, it sets HP to 0 and status to 'D', and clears `g_active_char` if the victim was the active member (`0x2a7f`-`0x2a9b`).
  5. It redraws (`0x2900`).
- The surface fountain is a different routine. LOOKOBJ:0x0162 prints `a gurgling fountain!\n\n` and `Who will drink?\n` (DS 0x729c/0x72b4), calls the raw picker 0x2e8e, and has no effect: `None!`, `Incapacitated!\n\n` or `Refreshing...\n` (DS 0x72c6/0x72ce/0x72e0). **`Who will drink?` never appears in a dungeon.**

### 1.9 RNG consumed by the Look body (CONFIRMED)

- `rand_range(1,255)` once, only for a lit class-0xC cell in a variant-3 dungeon. Note `dungeon.md` §8 says `rand(0,255)`, which is wrong: `push 1 / push 0xff`, and K:0x2092 is `(rng&0x7fff) % (hi-lo+1) + lo`.
- `rand_range(0,7)` once, only for `Y` at a 0x53-0x5F fountain.
- Nothing else. Picker idle animation draws are wall-clock driven and out of scope (`D86-FINAL.md` §7.5).

### 1.10 Cancel and abort paths (CONFIRMED)

| where | input | printed | effect | kernel return |
|---|---|---|---|---|
| `Player:` | ESC | `None!\n` | none | **1** |
| picker, zero G/P members | none | `None!\n` | none | **1** |
| picker, ineligible pick | that pick | `Disabled!\n\n`, then `Player: ` again | none | (re-ask) |
| darkness | none | `You see:\ndarkness.\n` | none | **1** |
| `Dir-` | Space | `Pass\n` | none | **1** |
| `Dir-` | ESC / other | nothing; the key is re-read | | |
| `Will you drink?` | anything but Y/N | nothing; the key is re-read | | |
| `Will you drink?` | N | `No.\n` | none | **1** |

### 1.11 Example transcripts (derived from the prints; the console layout is the device's)

Lit, no active member, two or more eligible, fountain 0x51 ahead:

```
Look...
Player: Iolo
Dir-Ahead
You see:
a fountain.
Will you drink?
Yes.  Gulp!
Healed!
```

The same in darkness: `Look...` / `Player: Iolo` / `You see:` / `darkness.`

With an active member set and a 0xB wall to the left: `Look...` / `Dir-Left` / `You see:` / `a wall.`

---------------------------------------------------------------------------------------------------

## 2 Time (CONFIRMED for the return value; the loop segment comes from `D86-FINAL.md` §2.3)

- Look returns **1** on every path: `K:0x317e` sets `[bp-2]=1`, the L arm never writes it, and `0x332f jmp 0x31ee` returns it. DUNGEON:0x06c4 passes it through (`0x07a6` → `0x0771` → `0x070a`).
- With `si=1` the loop runs the world turn **`DUNGEON:0x0c76`** at `0x0f84`. That turn does three things:
  - wakes sleepers with `rand(0,0x3f)` per 'S' member;
  - steps the wanderer, which can ambush;
  - dispatches the party-cell tile, redraws (K:0x2900) and runs housekeeping (K:0x2ae8).
- After that, the loop top charges `advance_clock(1)` (`0x0f2f`) before the next key.
- So Look (and a drink with it) costs exactly one dungeon turn and one minute. The drink's `rand(0,7)` is drawn **before** the world turn's draws. A `No.`, a `Pass`, a `None!` and darkness cost the same turn.
- **Errata.** `D86-FINAL.md:26` and `:338`, and the D-91 ledger row (`native/targets/tdeck/ALPHA4_UI.md:3752`), list "Look" among the keys that return 0 or "cost nothing". For Look that is wrong. Look is a turn-consuming command. Only the D-91 clock-placement nuance (housekeeping and clock *after* the command in the binary, *before* it in the ports) applies to it.
- `D` → `K:0x31ba` → `0x324e` gives `ax=DS 0xa14c` (`D-What?\n`), then `jmp 0x31e5`: print, then `0x31e9 [bp-2]=0`. That is **no world turn**, but the loop-top minute is still charged (the D-91 class).

---------------------------------------------------------------------------------------------------

## 3 DNGLOOK is the Look command, and other things too (CONFIRMED)

DNGLOOK.OVL is a grab-bag overlay. Thunks into overlay 10 from `thunks.py`:

| entry | role |
|---|---|
| 0x0000 | dungeon **Look** (§1) |
| 0x06a8 | View-a-gem flood (`docs/verdicts/gem-flood`, `dnglook-248-acta.md`) |
| 0x0844, 0x093a, 0x0c6c | room/marker helpers |
| 0x0d3e | camp/ambush synthetic arena |
| 0x0fda | frameless move dispatcher |
| 0x109e, 0x1130 | (not read) |
| 0x117e | `.CBT` scene populate |

The 3D view renderer is in DUNGEON.OVL (`forward_raytrace_column` 0x150a, per the `g_dng_map` ledger), not in DNGLOOK.

---------------------------------------------------------------------------------------------------

## 4 The fountain and `D` (CONFIRMED)

- The only consumers of the drink strings are DNGLOOK `0x013b` (DS 0x7700), `0x0206` (DS 0x7718) and `0x015e` (DS 0x7662). These come from an immediate-operand census over every OVL and ULTIMA.EXE. Fountains are drunk only through Look.
- `Here` (↓) targets the party's own cell, and the party can stand on a fountain: `move()` at DUNGEON `0x05fb`-`0x061b` blocks only classes 0xB, 0xC and 0xD, plus 0xA/0xF when backing up. So "drink from the cell you stand on" is reachable in 1988 as L, ↓, Y.
- The binary has **no `D`/drink command**. Native's `D` does not correspond to anything; the DOS answer to `D` is `D-What?` with no world turn.
- (SJOG `0x0760`, `Nothing hidden on the fountain.`, is the dungeon Search's text and is not a drink.)

---------------------------------------------------------------------------------------------------

## 5 Native and the TS reference today

### 5.1 Native core

- `native/core/include/openu5/dungeon.h:53-70`: `DungeonAction` has `Drink` (68) and `Search` (69, last), but no Look.
- `native/core/src/dungeon.cpp:377-416` (`DungeonAction::Drink`):
  - It acts on the **party's own cell** only, with no description, no question and no `Player:`.
  - The drinker is the **active member, else `party_members()[0]`**.
  - Off a fountain it prints `No fountain here.`, which is not in any binary (the QoL string from the reference).
  - The effect table matches §1.8: exact low nibble 0/1/2, else `rand(0,7)` with Damage and DamageScript events.
- `dungeon.cpp:198-200` prints `A fountain.` and `A chest!` when the party enters such a cell. **Neither string exists in any shipped binary** (I searched all `*.OVL`, `*.DAT` and ULTIMA.EXE), so they are PROBABLE fabrications. They are shared with TS `dungeon.ts:1481`. This is out of scope and only noted here.
- `native/core/src/dungeon_orchestration.cpp:185` limits the range check to `DungeonAction::Search`. `:192-193` calls `advance_turn(...,1,...)` before every non-Attack action, and `:206-216` runs `DungeonAction::Tick` afterwards unless the action moved the party. This is native's D-91-divergent model.

### 5.2 Native UI (UiSession; one implementation for the T-Deck and Windows)

`native/core/src/ui_session.cpp`:

- `:1510` `handle_dungeon`: the switch has no `l` arm, so **`L` falls to `:1625` and prints `What?`** with no command, turn or clock. This holds on the T-Deck and Windows, in both Original (`platform_dos_interface()`) and Enhanced modes.
- `:1575-1580` `case 'd'`:
  - It echoes `Drink`.
  - If `dungeon_fountain_here_` is set (`:1490-1496`, the **own** cell) it opens `begin_yes_no(DungeonDrink,"Will you drink?")`.
  - Otherwise it dispatches `DungeonAction::Drink`, which prints `No fountain here.` and charges a turn.
- `:666-675` resolves the answer. `Y` prints `Yes.  Gulp!` and dispatches Drink. `N` prints `No.` and **dispatches nothing (no turn)**.
- `:1561` `case 's'` (dungeon Search) reuses the `Dir-` target. The `:1172-1201` mapping gives `hours` 0 Ahead, 1 Here, 2 Left, 3 Right. The cancel arm `:1083-1099` sends `hours=2` for every dungeon prompt **except** Search. A new Look must be special-cased there, because 2 means Left in `Dir-`.

Windows (`native/targets/windows/src/app/desktop_input.cpp`):

- `:266` forwards characters unchanged to UiSession, so the behaviour is the same: `L` gives `What?` and `D` the shortcut.
- `:259`: in Enhanced mode with WASD movement on, `d` is a direction (East, which turns right) and never reaches the Drink shortcut.

The Original-mode record (`native/targets/windows/ORIGINAL_MODE_FIDELITY.md:250-251`, §7 item 4) already lists "Dungeon Look (DNGLOOK) missing, so the dungeon `D` (drink) shortcut stays".

T-Deck device layer (`native/targets/tdeck/main/alpha_runtime.cpp`, shared by Windows through `UV_TDECK_MAIN`):

- `:1063-1076` shows the pattern to copy for the 0x4988 seam: `resolve_command_char_or_prompt(SearchMember, ...)` with the pick parked in `pending_search_`.
- The answer arms are `:1640-1645`, the None and cancel arms `:1428-1455`, and the helpers `:1698-1720`.

### 5.3 TS reference (`game/src`)

The reference implements the chain.

- `main.ts:1685-1700`: `l` prints `Look...`, calls `pickCommandChar` (the 0x4988 model), applies the darkness gate (`dungeonCommand("look")`), then sets `pendingDungeonLook`.
- `main.ts:1566-1615`: the `Dir-` prompt. It runs `dungeonCommand("look",{target})` (`dungeon-cmds.ts:360`, `advanceTurn` at `:326` plus `turnTick`), then `fountainAhead(target)`, then the Y/N, then `dungeonDrinkAhead(target)` (`dungeon-cmds.ts:257`, no second turn).
- `dungeon.ts:772-799` `lookAhead` and `:168-195` `LOOK_DESC`/`FIELD_DESC` match §1.7, including 0x61, the fields, `rand(1,0xff)` and the variant.

It diverges from the binary in these ways (all CONFIRMED against §1/§2):

- **T1, wrong drinker.** `drinkFountain` (`dungeon.ts:1220`, `:1226`) uses `activeChar(state)`, not the `Player:` pick. The comment at `main.ts:1688` ("the Look does not use the index") is wrong: DNGLOOK `0x0229`/`0x023f`/`0x0257`/`0x026d` all index `[bp-0xc]`.
- **T2, cancel at `Player:` is free.** TS treats it as "aborta sin turno"; the binary returns 1 (a world turn).
- **T3, `Pass` at `Dir-` is free.** TS treats it as "aborta sin turno"; the binary returns 1. TS also accepts ESC as Pass (a declared ergonomic addition); the binary ignores ESC.
- **T4, wrong order.** The turn (advanceTurn and turnTick: sleeper `rand(0,63)`, wanderer `rand(0,3)`, possibly an ambush and corridor combat) runs **before** the Y/N and the drink. In the binary the drink, with its `rand(0,7)`, happens first and the world turn afterwards. The RNG order is therefore inverted, and an ambush can start while `Will you drink?` is still pending.
- **T5, the `d` shortcut** (`main.ts:1904`) is a declared QoL extra. `N` there costs nothing.

### 5.4 Fixtures, generators and tests that would see a change

- **`native/core/fixtures/dungeon.txt`**: generated by `native/core/tools/generate-dungeon-fixtures.ts` and checked by `native/core/tests/dungeon_parity_test.cpp`.
  - It covers actions 0..15, so **Drink (14) is pinned**: TS `drinkFountain(g)` on the own cell with the activeCharacter=4 member. Search (15) is driven with `dir 0` only.
  - Look is **not** covered. Adding it means appending columns: 18 hashes per row today, and the native loop runs `a<16`.
  - The snapshot hashes the event kind and text, so a change to Drink's text or member rule shows up.
- **`native/core/fixtures/dungeon-flow.txt`**: generated by `generate-dungeon-flow-fixtures.ts` and checked by `dungeon_flow_parity_test.cpp`. It covers orchestration turns for forward..attack, magic and the spell turn. Look and drink are not covered; the turn accounting for a Look would be added here.
- **TS tests**: `game/tests/dungeon-dispatch.test.ts`. Lines 207-285 cover `lookAhead` strings, `fountainAhead`/`drinkFountain(at)` and the `No fountain here.` pin. Lines 374-400 check that the shortcut and the Look give equal translator output.
- **Device tests**: `native/targets/tdeck/host_tests/dungeon_input_test.cpp:263` (D-IN-2f, "`d` dispatches DungeonAction::Drink"); the D-IN-4 `Dir-` blocks. The Windows Original transcript tests live in `native/core/tests/win_dos_interface_test.cpp`; none pins dungeon `L` or `D` today.
- **Unaffected**: `gameplay_parity` and `command_parity` exercise only the surface Look.

---------------------------------------------------------------------------------------------------

## 6 Recommendation

The reference is incomplete (T1-T4), so the order is: fix TS first at the layer the fixtures pin, run `--check` as the control, regenerate, and show the token diff. Then mirror it in native. Every new enum value is APPENDED.

### 6.1 Core (one rule, two dispatches, no awaiting flag in the core)

Description has to come before the question, and the drink has to come before the world turn. So the Look is split, with the UI holding the parked state, the way `pending_search_` works today. This deliberately avoids a core "awaiting" flag (see the Blackthorn softlock history in `GAMEPLAY_INTEGRATION_AUDIT.md:1214`).

**`DungeonAction::Look`** (appended after Search) takes `member` (-1 means `None!`) and `hours`:

| `hours` | meaning |
|---|---|
| 0 / 1 / 2 / 3 | Ahead / Here / Left / Right, exactly the Search encoding |
| `kLookNoDir` (-1) | no `Dir-` asked: darkness, or `None!` |
| `kLookPass` (4) | Space at `Dir-` |

The body, in order:

1. If `member < 0`, it prints nothing.
2. If it is dark (`!torch_turns && light_spell_minutes <= 0`, the same predicate Search uses at `dungeon.cpp:419`; put it in one shared helper), it prints `You see:\ndarkness.`
3. If `hours == kLookPass`, it prints nothing.
4. Otherwise it prints `You see:` plus the §1.7 table for the RAW byte at the `&7`-wrapped target. That includes the exact 0x61 remap, exact field bytes, `rand(1,255)` for variant 3, and the unreachable entries dropped.
5. If the class is 0x5, it emits a new appended `DungeonEventKind::DrinkPrompt`, translated to a new appended `GameEventKind::DungeonDrinkPrompt`.

**`DungeonAction::LookDrink`** (appended) takes `member`, `hours` = dir and a yes/no answer. Use a new defaulted `dungeon_action` parameter or `Command::cancel_target` for "No"; do not overload `hours`. It re-reads the target cell. On Yes with a fountain it applies the exact-tile effect to `member`: Cure 'G', Heal hp=max, Poison 'P', else `rand(0,7)` through `apply_damage` with DamageScript. It goes through one helper shared with the legacy `Drink`, so the two cannot drift.

**Orchestration (`dungeon_orchestration.cpp`):**

- Look keeps today's `advance_turn` before the action.
- If Look emitted DrinkPrompt, skip the Tick, rescue and refuge tail for that dispatch.
- LookDrink skips `advance_turn` and runs the action, then Tick, rescue and refuge.
- Every other Look path (None!, darkness, Pass, non-fountain) runs the normal tail.
- The result is exactly one clock tick and one world turn per Look, with the drink's draw before the Tick's draws.
- The D-91 clock placement stays as it is for all dungeon commands; it is a separate class fix.
- Widen the range check at `:185`.

Keep **`DungeonAction::Drink`** for now. Its value cannot move, and `dungeon.txt` pins it until the reference retires `dungeonCommand("drink")`. No UI path should reach it once the rest of this lands.

### 6.2 UI (UiSession, plus the AlphaRuntime picker seam; T-Deck and Windows share it)

**`l` in `handle_dungeon`:**

1. Echo `Look...`.
2. Raise the 0x4988 seam through a new appended `UiRequestId::LookMember`, copied from `SearchMember`:
   - **Resolved**: continue with that member.
   - **None** or **ESC**: print `None!` and **dispatch `Look{member=-1}`**, so the turn is still charged. This is different from Search's current "no turn".
   - **Disabled!**: ask again.
3. If it is dark (a shared predicate refreshed in `refresh_dungeon_context`), dispatch `Look{member, kLookNoDir}`.
4. Otherwise run `begin_target(Direction,"Dir-",Look{member})`, reusing the existing mapping. Space gives `Pass` and `Look{member,kLookPass}`, which charges the turn. Add Look to the special case in the cancel arm at `:1083-1099`. ESC is ignored in Original mode, as in the SJOG loop; Enhanced/T-Deck may treat ESC as Pass, which is a declared ergonomic choice and still charges the turn.

**`DungeonDrinkPrompt` event:**

- It opens `begin_yes_no(..., "Will you drink?")`. In Original mode it accepts only Y/N, with no cancel.
- `Y` prints `Yes.  Gulp!` and dispatches `LookDrink{yes}`. `N` prints `No.` and dispatches `LookDrink{no}`, which charges the turn.
- Retire the current `DungeonDrink` "N dispatches nothing" arm, or reuse its request id.

**`d`:**

- **Windows Original**: `D-What?` (K:0x324e), with no command. Today's "What?" keys charge nothing on native either; the D-91 minute is a separate matter.
- **T-Deck and Windows Enhanced**: optionally keep a convenience that is **exactly the `l` chain with `Dir-` preset to Here (↓)**. It would echo `Look...` so the transcript shows the rule that ran, and produce the same picker, darkness gate, description, Y/N, effects and turn. Off a fountain it then says `You see:\n<desc>` instead of `No fountain here.`; this is a visible T-Deck change. D-IN-2f still passes because an intent is still raised.
- Update ORIGINAL_MODE_FIDELITY §3.1 and §7 item 4 when this lands.

### 6.3 RED-first test plan

**Core tests (new `dungeon_look_test`, host, against the real `execute_dungeon_command`):**

1. **Strings, all 256 bytes.** Put each raw byte ahead of a lit party. Expect `You see:` plus the §1.7 text. Bytes 0x50-0x5F additionally emit DrinkPrompt and nothing else. Check 0x61 → `a passage.`, 0x69 → `a pit.`, 0x88 → `An energy field.`, 0xD0 → `a wall.` and 0xF3 → `a heavy door.`.
2. **Special wall.** In Shame/Hythloth expect `a caved in passage.`; in Despise/Destard/Doom expect `a dripping stalactite.`. In both cases the Look body draws no RNG (compare against a run without the wall). In Deceit/Wrong/Covetous expect exactly one `rand(1,255)` draw: pick a seed whose draw is 255 for `pirate`, and one whose draw is not for `adventurer`.
3. **Directions.** For each facing, Ahead/Here/Left/Right hit the right cell, including the toroidal wrap at x/y 0 and 7.
4. **Darkness.** With torch 0 and light 0, the output is `You see:` / `darkness.` only, no DrinkPrompt even on a fountain, and the turn is charged.
5. **The drinker is the pick.** Three eligible members, no active member, pick member 2 at a 0x52 fountain: only member 2 becomes 'P'. This kills the "active-or-first" mutant. Repeat for 0x50, 0x51 and 0x53 (Bad taste with damage 0..7 on member 2, death at 0 HP, `active_character` cleared, DamageScript slot 2).
6. **Whole-byte switch.** 0x59 gives `Bad taste.`, not Heal.
7. **Turn accounting on every path** (None!, darkness, Pass, non-fountain, fountain+N, fountain+Y): exactly +1 minute, exactly one Tick (sleeper `rand(0,63)` rolls and the wanderer step observed through the seed and wanderer position), and none on the DrinkPrompt dispatch alone.
8. **Order.** Fountain 0x53 with a sleeper in the party and an active wanderer: the damage equals the first draw after `advance_turn`, before the Tick's draws. Use a reference-RNG replay. The mutant that runs the Tick before LookDrink must go RED.
9. **Legacy Drink.** `dungeon.txt` stays green, which proves Drink is untouched; it is the control.

**UI and device tests (dungeon_input_test, D-IN-7; and win_dos_interface_test for Original mode):**

- `L` no longer prints `What?`.
- The full Original transcript of §1.11.
- `Player:` appears only with two or more eligible members and no active member.
- ESC at `Player:` gives `None!` and a turn.
- `Disabled!` asks again.
- Space at `Dir-` gives `Pass` and a turn. In Original mode ESC is ignored at `Dir-` and at `Will you drink?`.
- `N` gives `No.` and a turn.
- `D` gives `D-What?` in Original mode and the Look-Here chain on the T-Deck.

**Reference first:**

- Fix T1-T4 in `game/src` (`drinkFountain(state, at, member)`; a look split that runs the drink before `turnTick`; `Player:`/`Pass` dispatching the turn) with new rows in `dungeon-dispatch.test.ts`.
- Append Look columns to `generate-dungeon-fixtures.ts` and Look rows to `generate-dungeon-flow-fixtures.ts`. Run `--check` before regenerating; the token diff should show only appended tokens. Then extend the native drivers (`a<16` → the new range).

---------------------------------------------------------------------------------------------------

## 7 Side findings (not candidate E; recorded only)

- **Dungeon Search also always returns 1** (CONFIRMED at the kernel: `K:0x33a8`-`0x33be` → `jmp 0x31ee`). The native and TS "Pass at `Dir-` aborts Search without a turn" (`ui_session.cpp:1086-1097`) diverges the same way as T3.
- **Surface Search with `None!`** returns 1 at the kernel, and MAINOUT charges a turn for a 1 (`D86-FINAL.md` §2.6). AlphaRuntime's "no prompt, no effect, no turn" comment (`alpha_runtime.cpp:1703`) is PROBABLY a divergence. I did not re-read the MAINOUT loop.
- **`A fountain.` / `A chest!`** on-enter messages are in no shipped binary (§5.1). They are PROBABLE fabrications; I did not read the 0x0c76 tile dispatch to confirm that nothing is printed there.
- **Errata**: `dungeon.md` §8 should read `rand(1,255)`, not `rand(0,255)`. The D86-FINAL and ALPHA4_UI D-91 rows list Look as a no-turn key, which is wrong (§2). `main.ts:1688` says the Look does not use the picked index, which is also wrong.

## 8 Unknowns

- Static analysis only. There is no DOSBox keystroke trace for `L` in a dungeon (UNKNOWN in the dynamic sense; the bytes are unambiguous).
- The display side: how the device wraps or joins `Dir-` and the answer word on one row. I did not read the `0x1850` window driver.
- The COMBAT refusal `Look-Not here` was not re-derived (PROBABLE, from the native comment).
- DNGLOOK `0x109e` and `0x1130` were not read; they are irrelevant to Look.
