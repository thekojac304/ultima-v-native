# SPP1 Candidate A — world command turn costs: (R)eady, (U)se, (Z)-stats, (N)ew Order, getdir `Pass`

Read-only investigation, 2026-10-14. Every offset below was disassembled for this note with
`re/tools/dis16.py` and throwaway capstone scripts (`%LOCALAPPDATA%\Temp\spp1-A\`: `callers.py` with the
FITTED overlay bases, `intcallers.py`, `memrefs.py`, `strs.py`). `re/notes` and the TypeScript port were
read only as secondary claims to check.

Grades: **CONFIRMED** = I read the instructions; **PROBABLE** = inferred from code I read but not traced to
the end; **UNKNOWN** = not established. All findings are static (no DOS run).

**Provenance.** `original/u5/ultima5` is the Exodus Upgrade 1.0 install. The files that hold the dispatcher
and the three context loops are the ones with 2001 dates: `ULTIMA.EXE` (2001-08-20), `MAINOUT.OVL`
(2001-08-06), `TOWN.OVL` (2001-08-14), `DUNGEON.OVL` (2001-08-20). The command overlays (`ZSTATS`, `CMDS`,
`CAST`, `SJOG`, `TALK`, `LOOKOBJ`) are dated 1996. Everything here describes this binary set. Whether a
pristine 1988 `ULTIMA.EXE` has the same dispatcher return convention is **UNKNOWN** (see §9).

Address conventions: `K:xxxx` = ULTIMA.EXE CS offset (`dis16.py --exe`); overlay offsets are file offsets.
Overlay ids come from the filename table at ULTIMA.EXE image 0x8113 (1 TOWN, 2 MAINOUT, 3 DUNGEON, 4 INTRO,
5 FLAMES, 6 NPC, 7 COMBAT, 8 BLCKTHRN, 9 LOOKOBJ, 10 DNGLOOK, 11 OUTSUBS, 12 SHOPPES, 13 ENDGAME, 14 SJOG,
15 CMDS, 16 CAST, 17 TALK, 18 CAST2, 19 ZSTATS, 20 COMSUBS, 21 SHOPPES2, 22 SHOPPES3, 23 FONT). Every thunk
target and every resolved near call used below lands on a `55 8b ec` prologue (checked).

---

## 0. Verdict

1. **CONFIRMED.** The kernel dispatcher `K:0x3178` returns **1** to the context loop on **every** path of
   (R)eady, (Z)-stats, (N)ew Order and (U)se. The four handlers' own return values are discarded; the
   dispatcher returns its local `[bp-2]`, which is set to 1 in the prologue and never touched on those four
   paths. Cancels, refusals, empty lists, a single-member party — all return 1.
2. **CONFIRMED.** getdir `K:0x35EC` returns 0 on Space (`Pass`) and 1 on an arrow. **ESC is not accepted at
   all** (the read loop only leaves on Space or on codes 1–4). What a `Pass` costs depends on the command,
   not on getdir. For L, G, O, J, S, F, P (outside a dungeon) and T (overworld and dungeon) the dispatcher
   throws the result away and returns 1. Overworld K also returns 1 on every path. Town T, town K and town A
   return 1 on `Pass`. Overworld A returns 0 on every path (`MAINOUT 0x06ec` never sets its result to 1).
   Dungeon A and dungeon P return 0.
3. **CONFIRMED.** The three loops treat a nonzero result as one world turn and a 0 result as no turn
   (§3). The dungeon has one exception, already recorded as D-91: its 1-minute clock tick at
   `DUNGEON 0x0f2f` runs on every loop pass, before the key is read, whatever the result.
4. **CONFIRMED.** (R)eady costs exactly one turn per R command, however many items are readied or unreadied
   in the open `Item:` list. The list runs inside ZSTATS `0x0f2e`. The dispatcher returns once, when
   `0x1296` returns.
5. **CONFIRMED.** Native gives R, Z, N and U no world turn on any path. A world getdir cancel or `Pass`
   gives no turn either, except Klimb in a town. The TypeScript reference does the same. Both rest on the
   claim that these commands are "free actions" because ZSTATS never writes `g_unk_24e6`. But `0x24e6` is a
   repaint flag: its only read in the whole binary set is `K:0x595e`, inside the viewport redraw `K:0x5910`.

**Recommendation (§6).** Add one appended core command for "the dispatcher returned 1". The UI owner
(AlphaRuntime, plus UiSession for getdir) sends it **once**, when an R/Z/N/U interaction ends (any path) or
when a world getdir is answered with `Pass`. Leave `CommandKind::Ready`, `NewOrder` and `UseItem` free of
turns. That keeps the per-equip and per-pick semantics and every existing `gameplay_parity` row, and
nothing is charged twice. Do the TypeScript reference first.

---

## 1. The kernel dispatcher `K:0x3178`

```
3178: push bp / mov bp,sp / sub sp,4
317e: mov word ptr [bp-2], 1          ; default result = 1 ("a turn")
...
31e5: push ax / call 0x1850           ; print string
31e9: mov word ptr [bp-2], 0          ; the only generic 0-path
31ee: mov ax, word ptr [bp-2]         ; common exit
31f1: jmp 0x351c                      ; 351c: mov sp,bp / pop bp / ret 2
3231: mov word ptr [bp-2], ax         ; "return what the overlay returned"
3234: jmp 0x31ee
```

Key decoding (`0x3186`–`0x351a`, inline jump table for F..L at `0x3490`). The return rules, all CONFIRMED:

| Key | Handler | Strings (DATA.OVL, DS+0x10) | Return to loop |
|---|---|---|---|
| Space | `0x31f4` | "Pass\n" (0xa134); "Sheets in irons!\n" (0xa122) outdoors with `[0x5955]` set | **1** |
| A | `0x3216` | — | overlay ax: overworld MAINOUT `0x06ec` (thunk 0x7bae); town TOWN `0x09e6` (0x7aee); dungeon DUNGEON `0x1d4a` (0x7caa) |
| B / C / Y | `0x3236` / `0x3242` / `0x3464` | "Board " / "Cast...\n" / "Yell " | overlay ax |
| D, W, unknown | `0x324e` / `0x3450` / `0x34d8` | "D-What?" / "W-What?" / "What?" | **0** (via `0x31e5`) |
| E | `0x3254` | "Enter what?\n" off the overworld | overworld: overlay ax; elsewhere 1 |
| F, G, I, J, M, O, S, X | `0x3266`,`0x3274`,`0x32cc`,`0x32da`,`0x3340`,`0x335c`,`0x33a8`,`0x3456` | "Fire-", "Get-", ... | **1** (`jmp 0x31ee` after the call) |
| H | `0x3288` | "Hole up- ", "Only in bed!" | **1** |
| K | `0x32e8` | "Klimb-" | overworld CMDS `0x1c20` (0x809a) → **1**; town TOWN `0x0b82` (0x7aca) / dungeon DUNGEON `0x1e10` (0x7c86) → overlay ax |
| L | `0x3310` | "Look" + '-' (K:0x16ba is putchar) or "..." | **1** (dungeon DNGLOOK `0x0000` via 0x7f32; else LOOKOBJ `0x099c` via 0x7f26) |
| **N** | **`0x334e`** | "New Order" (0xa1c4) | **1** |
| P | `0x336a` | "Push-" / "Push\nNot here!\n" (0xa1d4) | **1**; **0** in a dungeon (`0x3378 → 0x31e5`) |
| Q | `0x338c` | "Quit:" | **0** (`jmp 0x31e9`) |
| **R** | **`0x339a`** | "Ready...\n\n" (0xa1f0) | **1** |
| T | `0x33c2` | "Talk-" / "Funny, no response!\n" / "Talk-Funny, no response!\n" | overworld 1; dungeon 1; town: TALK `0x041c` (0x8196) → 1 if ax==0, else **2** |
| **U** | **`0x340c`** | "Use item\n\n" (0xa24c) | **1** |
| V | `0x341a` | "View a gem!", "You have none!" | **1** |
| **Z** | **`0x3472`** | "Z-stats...\n" (0xa28c) | **1** |
| 0xFC | `0x31c6` | "Buffer On/Off" | **0** |

The four in scope:

```
339a: mov ax,0xa1f0 / push ax / call 0x1850   ; "Ready...\n\n"
33a1: call 0x7e4e                             ; thunk -> ovl 19 ZSTATS, target 0xf476 = ZSTATS 0x1296
33a4: jmp 0x31ee                              ; ax discarded -> returns [bp-2] = 1
334e: mov ax,0xa1c4 / push ax / call 0x1850   ; "New Order"
3355: call 0x805e                             ; ovl 15 CMDS, 0xcd5c = CMDS 0x0ddc
3358: jmp 0x31ee
340c: mov ax,0xa24c / push ax / call 0x1850   ; "Use item\n\n"
3413: call 0x7e42                             ; ovl 16 CAST, 0xd712 = CAST 0x1792
3416: jmp 0x31ee
3472: mov ax,0xa28c / push ax / call 0x1850   ; "Z-stats...\n"
3479: call 0x7e36                             ; ovl 19 ZSTATS, 0xec1a = ZSTATS 0x0a3a
347c: jmp 0x31ee
```

**`g_unk_24e6` is not a turn flag (CONFIRMED by a reference census).** Across ULTIMA.EXE and the 22
overlays there are 58 instructions with a direct `[0x24e6]` operand: 21 `mov ...,1`, 2 `mov ...,al`, 1
`mov ...,0`, 33 `or ...,2`, and exactly **one** read, `K:0x595e cmp byte ptr [0x24e6],0`, inside the
viewport redraw `K:0x5910`. No loop reads it.

---

## 2. Per command: paths → binary → loop → native → TS

"Loop" means the context loop's turn block (§3): overworld = MAINOUT `0x0c39`–`0x0d11`; town = TOWN
`0x15bf`–`0x1683`; dungeon = DUNGEON `0x0c76` (plus the loop-top minute, D-91).

### 2.1 (R)eady — ZSTATS `0x1296`

```
1296: push bp ... 129f: call 0            ; ZSTATS 0x0000 picker ("Player: " 0x96b4 -> K:0x2d7a)
12a2: mov [bp-4],ax / or ax,ax / jl 0x130b ; <0 -> return (picker already printed "None!" 0x96be)
12b8: call 0x5a4                         ; first owned item (list base 0x57c0, 0x30 ids)
12be: cmp ax,-1 / jne 12cc
12c3: mov ax,0x997e / call 0x3670        ; "Thou art empty-\nhanded!\n" -> jmp 130b (return)
12cc: "Item: " (0x9998) ... 12fc: call 0xf2e('R', member, first)   ; the open list
12ff..1308: redraws (K:0x4daa, 0x4e20, 0x4f3c, 0x2900)
130b: mov sp,bp / pop bp / ret
```

The list, ZSTATS `0x0f2e`, is a loop. It stays in the loop while `[bp-6]==0` (`0x1282: cmp [bp-6],0; jne exit;
jmp 0xf62`). For 'R', Enter or Space calls `0x0c5c` (try_equip_or_unequip, `0x121c`–`0x122b`) and
`[bp-6]` takes its return. `0x0c5c` returns 0 on every equip, unequip and refusal path. It returns 1 only
at `0x0e45`, "Ring vanishes!" (0x995e, items 0x2a/0x2c with `rand(0,15)==0` at `0x0e15`). ESC is
`0x1238`–`0x1250`: "Done\n" (0x9970), result -1, exit.

| Path | Binary | Dispatcher → loop | Native now | TS now |
|---|---|---|---|---|
| ESC at `Player:` ("None!") | ZSTATS `0x005b`–`0x0065` → `0x12a7` return | **1 → turn** | no turn (AlphaRuntime modal cancel, `alpha_runtime.cpp:1417-1457`) | no turn (`main.ts` doReady cancel arm, ~4740) |
| empty list ("Thou art empty-handed!") | `0x12c3` | **1 → turn** | no turn (`alpha_runtime.cpp:1918-1925`, Original only prints it) | no turn (`main.ts:4681-4688`) |
| ready / unready N items, then ESC ("Item: Done") | `0x0f2e` loop, `0x1238` | **1 → one turn**, whatever N is | no turn (`commands.cpp:1130-1155`: per-equip `Ready` command, explicitly "left to the picker owner") | no turn (`main.ts:4695-4728`; `game.ts:4337-4354` "ACCIÓN LIBRE") |
| "Ring vanishes!" | `0x0e45` → list exits | **1 → turn** | no turn (`alpha_runtime.cpp:1527-1530` closes the list) | no turn |
| refusal text (hands, armour, ammo, strength...) | `0x0c5c` → 0, list stays open | (part of the same command) | — | — |
| single-member party | ZSTATS `0x0000` still prompts via K:0x2d7a outside combat (only the `[0x5893]>0x80` arm skips it) | **1** | — | — |

### 2.2 (Z)-stats — ZSTATS `0x0a3a`

```
0a40: push 1 / call 0                     ; picker with arg 1 ('0' allowed -> -2)
0a4a: cmp ax,-2 / jne 0a56 / [bp-6]=6     ; '0' -> party page
0a56: cmp [bp-6],0 / jge 0a5f / jmp 0be9  ; ESC -> return (no "Done")
0a5f: "\nStatus: " (0x97a2) ... browse loop until Space (0x0a78) or ESC (0x0a81)
0bd6..0be6: redraws, "Done" (0x97ce)
0be9: mov sp,bp / pop bp / ret
```

| Path | Binary | → loop | Native | TS |
|---|---|---|---|---|
| ESC at `Player:` | `0x0a5c` → `0x0be9` | **1 → turn** | no turn (modal cancel) | no turn (modal skin) |
| browse, close with Space or ESC ("Done") | `0x0bd6`–`0x0bec` | **1 → turn** | no turn (`alpha_runtime.cpp:2661-2672`, "gameplay_command=none"; the arena alone yields) | no turn |
| '0' → party page | `0x0a4f` | **1** | no turn (`alpha_runtime.cpp:1494-1497`) | no turn |

### 2.3 (N)ew Order — CMDS `0x0ddc`

```
0de4: "\n\nSwap " (0x4318) / call K:0x2e8e (= K:0x2d7a(0))
0df1: inc ax / jne 0dfe / "nobody!\n" (0x4320) / jmp 0ead
0e0c: cmp [bp-0x22],0 / jne 0e26 / "\n\n" + name + " must lead!\n" (0x432c) / ret
0e26: "\nwith " (0x4339) / second pick: -1 -> "nobody!\n" (0x4340); 0 -> " must lead!\n" (0x434c)
0e64: "!\n" (0x4359) / swap the two 0x20-byte records / [0xa9fa]=1 / ret
```

| Path | → loop | Native | TS |
|---|---|---|---|
| ESC at `Swap` ("nobody!") | **1 → turn** | no turn (modal cancel; `\nSwap nobody!`) | no turn |
| Avatar first or second ("must lead!") | **1 → turn** | no turn (`commands.cpp:774-780`) | no turn (`game.ts:4325-4335` "Sin RNG, sin turno") |
| ESC at `with` | **1 → turn** | no turn | no turn |
| completed swap | **1 → turn** | no turn | no turn (`main.ts:6224-6240` "ACCIÓN LIBRE") |
| single-member party | K:0x2d7a still prompts → "must lead!" or "nobody!" | **1** | — | — |

### 2.4 (U)se — CAST `0x1792`

```
17b2: call 0xffffc012 (thunk 0x7f92 -> ZSTATS 0x05a4)  ; first usable item, list 0xb9ee, 0x26 ids
17b8: cmp ax,-1 / jne 17c8 / "No usable items!\n" (0x489f) / jmp 1baa (return)
17c8: "Item: " (0x48b1), "Items:" banner (0x48b8)
17f2: call 0xffffc1f2 (thunk 0x8172 -> ZSTATS 0x0f2e('U', 0xff, first))
1804: cmp [bp-0x12],0 / jge / jmp 1baa      ; ESC -> "None!" (0x9976) printed by 0x0f2e, return
180d..1b5b: per-item handlers ([bp-0xa] = success flag)
1b8a: cmp [bp-0xa],0 / jne 1baa / "Failed!\n" (0x4a7b) + sound
1baa: ret
```

There is no `Player:` step. Use is party-wide, and the list is entered with member 0xff.

| Path | → loop | Native | TS |
|---|---|---|---|
| "No usable items!" | **1 → turn** | no turn (`alpha_runtime.cpp:1918-1925`) | no turn (`main.ts:4818-4823`) |
| ESC at the list ("Item: None!") | **1 → turn** | no turn (modal cancel) | no turn |
| any item, any outcome ("Not here!", "No stars!", "Failed!", success) | **1 → turn** | no turn (`commands.cpp:1074-1129` "do not consume a turn"; `dungeon_orchestration.cpp:158-160`) | no turn (`endgame/use-tools.ts:12` "el (U)se no rueda turno en el port") |
| potion/scroll target "On who:" cancelled, skull-key or scroll getdir `Pass` | **1 → turn** (inside the same U) | no turn | no turn |

### 2.5 getdir `K:0x35EC` and its `Pass`

```
35f2: [0x5876]=0 / [0x5878]=0 / jmp 3618
3600: cmp [bp-2],3 / je 3622 ... cmp 4 / cmp 1 / cmp 2 / je 3622
3618: call 0x266c / mov [bp-2],al / cmp al,0x20 / jne 3600   ; only Space or 1..4 leave the loop
3645: "Pass\n" (0xa2a0) / sub ax,ax / ret        ; Space -> 0
3650..3687: "North/South/West/East\n", dx/dy in [0x5876]/[0x5878], ax=1
```

The `cmp ax,0x1b` at `0x363b` cannot be reached, because the loop above never lets ESC through
(CONFIRMED). So a world direction prompt has no ESC cancel in this binary. Its only "no direction" answer
is Space, printed `Pass`.

What each caller does with the 0 (CONFIRMED unless marked):

| Command (where) | Code after getdir==0 | Dispatcher → loop | Native now (device Cancel / Original Space) | TS now |
|---|---|---|---|---|
| Look (overworld, town) | LOOKOBJ `0x099c` (getdir at `0x09a2`) returns; dispatcher `0x333c jmp 0x31ee` | **1 → turn** | no turn (UiSession cancel → ModalResponse dropped by `AlphaRuntime::modal`) | "Cancelled.", no turn (`main.ts:6008-6016`) |
| Get, Open, Jimmy, Search | SJOG `0x18ce`/`0x1374`/`0x0d4a`/`0x095c` (getdir `0x18ea`/`0x139f`/`0x0d78`/`0x097e`) | **1 → turn** | no turn | no turn |
| Push (not dungeon) | CMDS `0x161a` (getdir `0x1632`) | **1 → turn** | no turn | no turn |
| Fire | CMDS `0x0aea` → `0x0962` (getdir `0x0982`) | **1 → turn** | no turn | no turn |
| Talk, overworld | kernel `0x33d0` getdir; `0x33d5 jne` not taken → `0x31ee` | **1 → turn** (a direction also gives "Funny, no response!" and 1) | no turn | no turn |
| Talk, town | TALK `0x0422`: `or ax,ax / jne / sub ax,ax / jmp 0x4ce` → 0 → dispatcher `0x33fe jne` not taken | **1 → turn** | no turn | no turn |
| Klimb, overworld | CMDS `0x1c46` getdir; `0x1c4d jmp 0x1d05` (ret); dispatcher `0x32f9 jmp 0x31ee` | **1 → turn** — and so are "With what?" (0x9016), "On foot!" (0x9022), "Impassable!", "Not climbable!" | **no turn** (`commands.cpp:1294-1299` KlimbCancel is NoOp at location 0) | **no turn** (`game.ts:4200-4205`: "su getdir (CMDS 0x1c4d) termina SIN turno" — misses the dispatcher's 1) |
| Klimb, town | TOWN `0x0be3` getdir; `0x0be8 je 0x0c3e` → `[bp-2]=1` → dispatcher `0x3306 jmp 0x3231` | **1 → turn** ("What?" `0x0c38` and "-On foot!" `0x0ba4` → 0, no turn) | turn (KlimbCancel → `r.turn()`) ✓ | turn (`klimbCancel`) ✓ |
| Attack, overworld | MAINOUT `0x06ec`: `[bp-2]=0` at `0x06f2`, never set to 1; getdir `0x0732`, `0x0737 je 0x0789` | **0 → no turn** on every path (Pass, "On foot!", "Nothing to attack!", a fight) | no turn ✓ | no turn ✓ |
| Attack, town | TOWN `0x09e6`: `[bp-4]=1` at `0x09ed`; getdir `0x0a24`, `0x0a29 jmp 0x0b79` returns 1 | **1 → turn** (only "On foot!" `0x0a14`–`0x0a1b` → 0) | no turn | no turn |
| Attack, dungeon | DUNGEON `0x1d4a`: no getdir (attacks ahead); `[bp-2]=0` always | 0 | (dungeon Attack charges nothing) ✓ | ✓ |

---

## 3. The context loops: what one "turn" is

All three loops call the dispatcher exactly once: MAINOUT `0x0c00`, TOWN `0x158f`, DUNGEON `0x07a3`. These
are the only three E8 call sites to K:0x3178 in the binary set (CONFIRMED by a full call census with the
fitted bases).

### 3.1 Overworld — MAINOUT `0x0a84` loop

```
0a8f: [bp-0xa]=0 / 0a94: [bp-8]=1           ; result defaults to 1 every pass
0aa2: call K:0x39fc  ; 1 = all asleep -> "Zzzzzz..." and jmp 0x0c12 with [bp-8]=1 (a turn)
0b14: call 0x598 (key) ; <0x20 -> control table 0x0bbe; '0'..'9' -> K:0x4080; else ->
0c00: call K:0x3178 / 0c0f: mov [bp-8],ax
0c20: cmp [0x5893],0 / je 0c30 / [bp-0xa]=1 / jmp 0d14   ; location changed -> leave, no turn
0c30: cmp [bp-8],0 / jne 0c39 / jmp 0d14                  ; 0 -> skip the whole block
0c39: push 2 / call K:0x4f7c                              ; advance_clock(2)
```

What runs on a nonzero result, in order (CONFIRMED):
1. `K:0x4f7c advance_clock(2)`. If `g_time_spell [0x587a]=='Q'` the minutes are halved (minimum 1, `0x4f8d`);
   if it is `'T'`, no minutes are added (`0x4fa6`). Torch and light counters `0x58a7`/`0x58a6` are decremented
   by the minutes; hour, day and month roll over; the Shadowlord re-roll happens at midnight.
2. Tile hazards under the party (`0x0c40`–`0x0ccc`): tile `0x6a/0x6b` → `0x1be8`; swamp (tile 4, on foot)
   → thunk `0x7b4e`; lava `0x8f` → thunk `0x7b5a`; the (0xe9,0xeb) special tile.
3. `0x0a60`: off the surface floor, `rand(0,255)==0x69` → "EARTHQUAKE!".
4. **`K:0x2ae8` housekeeping** (`0x0cd3`): poison tick for each 'P' member (K:0x2a52), active-player reset
   on death. On an hour change, "Starving!" or a meal at 6, 12 or 18 (K:0x3f54). `K:0x3ef0(0xff,1,&0x588b)`
   (the saturating turn counter that native calls `turns_since_start`). The time-spell countdown `0x588e`
   (→ clears `0x587a`). Ring regeneration `K:0x400c` (`rand(0,7)==7`).
5. Rough seas (tile 1 on a ship), tile `0xd4`-class → thunk `0x7b42`.
6. **`0x1a60` world phase** (`0x0d11`): none under 'T'; every other turn under 'Q' (`0x2c55`) or when
   mounted (horse or carpet tiles 0x12–0x15, `0x2c57`). Then the encounter roll `rand(1,30)` vs `0xd8c()` →
   `0xfc4` spawn, a step for each live outdoor monster (`0x131a`/`0x198c`), and despawn beyond 32.

A 0 result skips all six. The per-pass work that does not depend on the result is `0x0a1a` (tile-0xff
check, `advance_clock(0)` = light-level refresh only), K:0x39fc and K:0x48a8 (the moongate check).

### 3.2 Town — TOWN `0x141e` loop

```
142c: [bp-0xc]=0 / 1431: [bp-0xa]=1
1436: call K:0x39fc ; 1 -> "Zzzzzz..." (0x288d) and jmp 15a6 with [bp-0xa]=1 (a turn)
1492: call 0xdc4 (key) -> [bp-6]; control table 0x1552; digits -> 0xe34; else
158f: call K:0x3178 / 159a: mov [bp-0xa],ax
159d: cmp [bp-0xa],3 / jne / jmp 148f       ; 3 = re-read the key, no turn
15a6: cmp [0x5893],0 / jne 15b6 / [bp-0xc]=1 ; left for the overworld -> no turn
15b6: cmp [bp-0xa],0 / jne 15bf / jmp 1686  ; 0 -> no turn
15bf: call K:0x39fc / inc ax / jne 15c8 / jmp 1686   ; nobody conscious -> no turn
15c8: save hour / 15d0: push 1 / call K:0x4f7c       ; advance_clock(1)
```

What runs on a nonzero result (CONFIRMED unless marked):
1. `advance_clock(1)` (same Q/T rules).
2. On an hour change to 20 or 5 → TOWN `0x0170` (PROBABLE: hour-tile refresh; native `RefreshHourTiles`).
3. **TOWN `0x0f02`**: sleepers wake (`rand(0,15)==15`, 'S'→'G'). A trapdoor (tile 0x8c, "A TRAPDOOR!")
   drops the party and loops. A swamp poisons ("Poisoned!", `rand(0,29)` vs dex, on foot). Fire or lava
   (0xbc/0x8f) burns ("Burning!"). Then **`K:0x2ae8` housekeeping at `0x10d0`**.
4. The `[0x594f]`/`[0x5952]` countdown → K:0x39cc (UNKNOWN purpose), and the avatar slot sync (`0x160d`).
5. The NPC tail. It is skipped every other turn when mounted (tiles 0x12–0x15) and the key is not Space
   (`0x161f`–`0x1640`; the key is `[bp-6]`, so a getdir `Pass` inside L/G/... counts as **not** Space). It
   is skipped entirely under 'T' and every other turn under 'Q'. The tail is: TOWN `0x0c78` (objects of
   tile 0x10/0x11); if result < 2, thunk `0x7ab2(hour)` → NPC.OVL `0x0db4` (PROBABLE: the NPC movement and
   schedule turn); if `[0x65bf]` is set or the result is 2, TOWN `0x1352(result-1)` (PROBABLE: the NPC
   engine's engaged-NPC second turn).

### 3.3 Dungeon — DUNGEON `0x0e2e` loop

```
0fa8: call K:0x39fc ; 1 -> "Zzzzzz..." and back to 0f93 (no clock, no key); -1 -> leave
0fd6: 'T' -> 0f1e: di=0, no clock;  'Q' -> 0f1e: di^=1, clock only when di becomes 1
0fea: di=1 / ax=1 / jmp 0f2e -> 0f2e: push ax / 0f2f: call K:0x4f7c   ; advance_clock(1) EVERY pass
0f36: call 0x3d6 (key) / 0f42: call 0x6c4(key) / mov si,ax   ; 0x6c4 -> 0x07a0..0x07a6 -> K:0x3178
0f47: left the dungeon -> si=0
0f7b: or si,si / je 0f87 / push cell / push di / 0f84: call 0xc76   ; nonzero -> the turn block
```

`0x06c4` returns the dispatcher result unchanged for every letter key (`0x07a3 call`, `0x07a6 jmp 0x0771
mov [bp-2],ax`). Digits are forced to 0 (`0x07d1`, D-86). R, Z, N and U are letters, so they return 1.

The turn block `0x0c76(di, cell)` (CONFIRMED): sleepers wake (`rand(0,0x3f) < 4`). If `di`, the monster
phase `0x07e2` runs (→ `0x0b7e` when engaged). Cell effects for cell classes 0xf0/0xa0. `K:0x2900` status
redraw (`0x0e1f`). **`K:0x2ae8` housekeeping (`0x0e22`)**. The clock minute at `0x0f2f` is **not** gated by
the result; that is D-91.

### 3.4 Answers to questions 2–4

* **A 0 return skips everything turn-shaped** in the overworld and in towns: clock, hazards, housekeeping,
  NPC and monster phase, encounter roll (CONFIRMED). In the dungeon it skips the `0x0c76` block but **not**
  the loop-top minute (D-91, CONFIRMED).
* **One turn = one call of K:0x2ae8** in every context, plus that context's clock (overworld 2, town 1,
  dungeon 1 at loop top) and that context's world phase (overworld `0x1a60`, town NPC tail, dungeon
  `0x07e2`).
* **Ready charges one turn per R command** (CONFIRMED, §2.1). The open list is a loop inside the overlay,
  and the dispatcher returns once.
* **Context differences** for R/Z/N/U: none at the dispatcher (no `[0x5893]` test on those four paths).
  The cost differs only as each loop's turn differs (overworld 2 min + world phase; town 1 min + NPC tail
  + mount/Q/T phases; dungeon `0x0c76` + loop minute). For getdir `Pass` the command matters: A is free
  outdoors and charged in towns; P is free in dungeons; T and L are charged everywhere.
* **Wind is not part of this decision** (PROBABLE). The only wind roll, `K:0x2f62`, is called solely from the
  viewport redraw `K:0x5910` (`0x5944`), which the key reader `K:0x266c` (`0x269a`) also calls while idle.
  It is gated by `[0x58a4]`/`[0x5891]`, not by the dispatcher result. Native and TS roll wind at the start
  of `outdoor_turn`/`town_turn`, an existing model choice outside this candidate. One consequence: any new
  native turn also adds the 1/64 wind draw to the RNG stream.
* **Arena** (out of scope): COMBAT.OVL has its own dispatcher. Native already charges the arena turn once
  at Ready-picker close (`ui_session.cpp:560-605`, `CombatYield`) and on the Original-mode Use, Z and
  Target paths.

---

## 4. Native and TS today, and where the end of each command lives

Native core (no turn anywhere on these paths):
* `native/core/src/commands.cpp:774-783`: NewOrder and SetActivePlayer block, no `r.turn()` for NewOrder.
* `commands.cpp:803-810`: "A dungeon CORRIDOR Ready is an ordinary free action".
* `commands.cpp:1074-1129`: UseItem, "do not consume a turn". `world_magic.cpp:15-37` (potions, scrolls,
  items ≥16) and `quest_world.cpp:119-155`, `:348-356` charge nothing either.
* `commands.cpp:1130-1155`: Ready. Its comment already says the cost is "a picker-lifecycle event … Left
  to the picker owner".
* `commands.cpp:1294-1299`: KlimbCancel, `r.turn()` only when `position.map.location != 0`.
* Precedents for a "charge the context turn" command: `commands.cpp:785-790` (`AfterGemView`: `r.turn()`
  on the surface and in towns, `advance_turn(1)` in a dungeon) and the dungeon's `DungeonAction::Tick` for
  refusals that cost a turn (`ui_session.cpp:1597-1608`; `dungeon_orchestration.cpp:190-207`).
* `Runner::turn` `commands.cpp:199`. `turn.cpp:79-104` `housekeeping_into` (`++turns_since_start` at `:96`),
  `:156` `outdoor_turn`, `:196` `town_turn`.

Native UI, the points where each interaction ENDS:
* R starts at `ui_session.cpp:1452` (dungeon `:1612`), then `AlphaRuntime` `alpha_runtime.cpp:1007-1017`.
  It ends at: the EquipmentMember cancel and the Equipment cancel (`AlphaRuntime::modal`
  `alpha_runtime.cpp:1417-1457`, "Player: None!" / "Item: Done"); the empty list (`:1918-1925`); the
  ring-vanish close (`:1527-1530`). Per-equip `Ready` commands reopen the list (`:1522-1532`) and are NOT
  ends. `UiSession::cancel_modal` (`ui_session.cpp:532-605`) is where the combat-only charge already sits;
  its comment states the world rule wrongly ("Overworld/town/dungeon Ready has no cost at all").
* Z starts at `ui_session.cpp:1469` (dungeon `:1615`). It ends at the Status cancel (`modal`) and at the
  viewer close (`alpha_runtime.cpp:2661-2672`; the '0' path opens the viewer at `:1494-1497`).
* N starts at `ui_session.cpp:1449` (dungeon `:1597`). It ends at the Party cancel ("Swap nobody!" / "with
  nobody!", `modal`) and after the `NewOrder` command (`alpha_runtime.cpp:1472-1490`).
* U starts at `ui_session.cpp:1455` (dungeon `:1613`). It ends at: the Inventory cancel and the empty list
  (`modal`, `:1918-1925`); the `UseItem` command dispatched from `:1507-1521` (directly, after a UseTarget
  pick, or after the item getdir); the UseTarget cancel (`modal`, including the Original-mode spent-potion
  command at `:1440-1443`); the item getdir cancel (`ui_session.cpp:1116-1117`, `use_item` re-dispatch).
* World getdir: the Cancel/Back branch is `ui_session.cpp:1083-1122` (Klimb → `KlimbCancel`; the other
  world commands → a dropped ModalResponse). Original-mode Space goes through `dos_getdir_pass`
  (`:1375-1391`) into the same branch.

TS reference (also no turn):
* `game/src/main.ts:4677-4750` (`openReadyPicker` / `doReady`), `:4816+` (`openUsePicker`),
  `:6224-6240` (N, "ACCIÓN LIBRE"), `:1724-1745` (dungeon r/m/u/n), `:5978-6016` (getdir cancel →
  "Cancelled.", turn only for `klimbCancel`). Z is a modal skin with no core call.
* `game/src/core/game.ts:4325-4335` (`newOrder`, "sin turno"), `:4337-4354` (`readyItem`, "ACCIÓN LIBRE …
  g_unk_24e6"), `:4200-4205` (`klimbCancel`, overworld excluded), `endgame/use-tools.ts:12`.
* `re/deliberate-divergences.md:287-295` (Ready "free action … g_unk_24e6") and `:420-436` (only town
  klimb charges; "the other 7 directionals → Class C").

---

## 5. Fixtures and suites that would observe a change

* **`gameplay_parity`** (`native/core/tools/check-gameplay.ts` vs `tests/gameplay_driver.cpp`; it projects
  `time`, `turnsSinceStart`, the seed and more). Rows that issue the in-scope core commands: `use`
  (`check-gameplay.ts:106` carpet ×24, `:107` skull key ×10, `:110` spyglass ×24, `:111` potions and
  scrolls ×256) and `order` (`:131`, 36 rows). If the turn is put **inside** `UseItem`/`NewOrder`, all of
  these move and the harness's inline TS `use` code (`:74-76`) must change in lockstep. Under the
  recommended epilogue design they stay byte-identical, and new rows (`[{order},{turn}]`, ...) pin the
  epilogue in both ports.
* **`command_parity`** (`fixtures/commands.txt` from `tools/generate-command-fixtures.ts`): all
  `klimbCancel` scenarios (18, 31) are local. Fixing the outdoor KlimbCancel moves no existing row, so a
  location-0 row is needed.
* `turn_parity` / `turns.json`: unaffected (the primitives do not change).
* Native unit tests that pin "free": `tests/batch3_group_c_test.cpp:156-168` (C2, dungeon `Ready` command
  costs no turn; stays true per command under the epilogue design) and `:847-858` (C12, combat member
  cancel, out of scope). The comment block in `ui_session.cpp:560-576`.
* Windows Original scenario `native/targets/windows/scenarios/original_interface.txt:20-110` drives
  exactly these flows (R / Z / U / N / `Look-Pass`) and already supports `expect remember_turns` /
  `expect turns_advanced` (`:143-149`). It is the natural place for runtime assertions.
* TS: `game/tests/ready-rng.test.ts` ("free turn"), `tests/town-stairs.test.ts` (klimb cancel), and the e2e
  specs for these pickers.
* Host runtime template: `native/targets/tdeck/host_tests/a4_parity2_d86_dungeon_digit_runtime_test.cpp`
  (real AlphaRuntime, raw keys, a `Snap` of turns, seed, clock, food, HP and dungeon cell).

---

## 6. Recommendation: the narrowest correct layer

**Core (native, then TS mirror):** append one command kind at the end of `CommandKind` (after
`CampAcknowledge`), e.g. `DispatcherTurn`, meaning "the kernel dispatcher returned 1 for a non-Space key".
* Overworld and town: `r.turn()` with `pass=false`. That is the outdoor block with 2 minutes plus `world()`,
  or the town block with 1 minute and the mount/Q/T phases.
* Dungeon: the existing `DungeonAction::Tick` path (`advance_turn(1)` + Tick), the same as the dungeon's
  other "refusal with a turn" keys. D-91 (the loop-top minute for 0-results) stays its own row.
* It must pass the long context gate in `execute()` (`commands.cpp:812-818` lists the kinds allowed past
  `UseItem`), or the UI must route the dungeon case as `DungeonCommand{Tick}`.
* `KlimbCancel` at location 0 should charge `r.turn()` too (binary CMDS `0x1c20` + dispatcher `0x32f9`).

**UI (the only layer that sees the whole interaction):** AlphaRuntime sends `DispatcherTurn` **exactly once**
when an R/Z/N/U interaction that began outside an arena ends, on every END point listed in §4. Two ways
to build it:
(a) a latch set when the R/Z/N/U intent arrives and consumed at the first moment the UI is back in its base
mode with no selection, no zstats viewer and no pending R/U/N state. This is the binary's own funnel and
catches future paths.
(b) explicit calls at each listed END point.
I recommend (a), with (b)'s END list as the test matrix. UiSession's world-getdir Cancel/Pass branch
(`ui_session.cpp:1104-1122`) sends `DispatcherTurn` for L, G, O, J, S, P, F, T, and for A only in a town.
Klimb keeps `KlimbCancel`; overworld Attack sends nothing.

**Why not inside `Ready` / `NewOrder` / `UseItem`?** Ready is per equip in native, so one R would cost N
turns. The cancels of R, Z, N and U never reach the core at all. Putting the turn in those commands also
rewrites 350 `gameplay_parity` rows. With the epilogue nothing is charged twice, because none of these
commands or their helpers calls `r.turn()` today (§4).

**Order (batch workflow):** binary first (this note). Then the TS reference at the layer the fixtures pin: a
`game.dispatcherTurn()` that equals `runContextTurn({consumed:true})` on the surface and in towns and
`dungeonSpellTurn()` underground; the main.ts call sites; correct `klimbCancel` at location 0; correct the
"ACCIÓN LIBRE"/g_unk_24e6 comments and the two `deliberate-divergences.md` rows. Then `--check`,
regenerate and token-diff. Then native.

**Owner decisions to record (UNKNOWN, not derivable from the binary):**
* Should a **device Cancel** at a world getdir on the T-Deck (and in Enhanced mode) equal the binary's Space
  `Pass`, and so cost a turn? The binary has no ESC there. The arena precedent kept free cancels on the
  T-Deck as a declared handheld contract. Original mode (Space = `Pass`) should charge without question.
* R/Z/N/U on the T-Deck: no handheld contract was ever declared for them (their "free" status came from the
  g_unk_24e6 reading), so the shared rule should apply. That is a deliberate T-Deck behaviour change and
  needs a ledger row. Use the preservation differential to prove that nothing else moves.

---

## 7. RED-first test plan

Snapshot every case as the D-86 runtime test does: `turns_since_start`, the seed, the clock, food,
HP/status, `time_spell`/`spell_turns`, NPC positions, and the dungeon cell/wanderer. Each assertion should
go RED against the current tree.

Core (new `native/core/tests/spp1_a_dispatcher_turn_test.cpp`; TS twin in vitest):
1. `DispatcherTurn` overworld: turns +1, minutes +2. With 'Q': +1 minute and the world phase on alternate
   turns. With 'T': +0 minutes and no world phase. The spawn gate is rolled (`result.world_turns == 1`).
2. Town: turns +1, minutes +1, the NPC tail ran (`world_turns`, an actor moved). When mounted, the tail is
   skipped on alternate calls (contrast with `Pass`, which ignores the mount phase).
3. Dungeon (via Tick): turns +1, minute +1, the wanderer stepped. A poisoned member loses 1 HP.
4. `KlimbCancel` at location 0: turns +1, minutes +2 (RED today: NoOp).
5. Housekeeping riders on any of 1–3: food −eaters when the call crosses 06:00/12:00/18:00; "Starving!"
   with food 0 on an hour change; `spell_turns` −1.

Runtime (real AlphaRuntime, raw keys; Windows Original scenario lines for the dos model). For each row,
expect **exactly +1** turn (and +2 or +1 minutes by context):
6. R: ESC at `Player:`; empty member ("Thou art empty-handed!"); equip 2 items then ESC (+1, not +2/+3);
   ring vanishes; single-member party on the T-Deck (the list opens directly) then Back.
7. Z: ESC at `Player:`; open, page, Space; open, ESC; '0' then close.
8. N: `Swap` ESC; Avatar first ("must lead!"); a valid swap; `with` ESC; one-member party.
9. U: "No usable items!"; list ESC ("Item: None!"); spyglass at noon ("No stars!"); potion with the
   "On who:" ESC; skull-key getdir cancel; scroll 1 (Rel Hur) getdir cancel.
10. Getdir `Pass` (Original Space, plus device Cancel if the owner chooses): Look, Get, Open, Jimmy,
    Search, Push, Talk, Fire in the overworld (+2 min) and in a town (+1 min). **Controls:** overworld
    Attack-Pass +0; town Attack-Pass +1; town Klimb "What?" +0; town Klimb-Pass +1 (already green);
    overworld Klimb-Pass +1.
11. Negative controls that must stay 0: RETURN/ESC "What?" (`original_interface.txt:142-149`), D-What,
    W-What, a dungeon digit (D-86), a dungeon Push ("Not here!").
12. Dungeon R/Z/N/U: +1 turn, the wanderer stepped, the loop minute counted once (not twice).
13. Parity: new `gameplay_parity` rows `[{order},{turn}]`, `[{use},{turn}]` and `[{look},{turn}]` in a town
    and on the surface, in both ports; a location-0 `klimbCancel` row in `generate-command-fixtures.ts`.

Mutation targets: drop the epilogue (all of 6–10 go RED); send it twice for R (row 6 "equip 2" goes RED);
send it for overworld Attack (control in 10 goes RED); use `pass=true` in towns (row 2's mount alternation
goes RED).

---

## 8. Adjacent findings (out of scope, recorded for the batch)

* **Completed** Look, Get, Open, Jimmy, Search, Push, Fire and Mix return 1 on **every** outcome, refusals
  included. So do H (camp, "Only in bed!") and V (`0x344a`, already ported). Native charges a turn only on
  some outcomes or none: `world_look` (`look.cpp:59`) never calls `r.turn()`; `get_quest_object` returns no
  turn for "Open it first!", "Can't reach plate!" and "Nothing to get!" (`quest_search.cpp:33-58`). TS
  `look()` charges nothing either. Same class as this candidate, larger fixture footprint
  (`check-gameplay.ts:108` alone is 1280 Look rows).
* Town Talk with no NPC is TALK → 0 → dispatcher 1 → a turn. *(Correction, SPP1 2026-10-14: the town text is
  not "Funny, no response!" but TALK `0x048a` (no NPC) → `0x0491` DS 0x91d6 `"\nNobody's here!\n"` → `0x0429`
  ax=0. "Funny, no response!" is the kernel arm's overworld/dungeon string, which native also prints in a town:
  ledger D-111.)* Native returns
  `Rejected` with no turn (`dialogue_orchestration.cpp:136,145`).
* Overworld K without a grapple, or not on foot ("With what?", "On foot!") → a turn in the binary. Native and
  TS: none.
* D-101's open point "the dungeon consumer is untraced" is answered for Use: DUNGEON `0x07a3` → `si=1` →
  `0x0c76` (housekeeping `0x0e22`, wanderer `0x07e2`). Cast returns the overlay's value (`0x3249 jmp
  0x3231`), which this note did not trace.
* In this binary the `Player:` picker K:0x2d7a moves a highlight with digits and arrows; Enter or Space
  confirms, ESC → -1, '0' → -2 when allowed. Possibly an Upgrade-1.0 UI change (UNKNOWN), and irrelevant to
  turn cost.
* `re/deliberate-divergences.md:431-433` says the getdir key set "lives in the resident kernel not dumped".
  It is dumped above: Space or an arrow code; ESC is ignored.

---

## 9. UNKNOWNs

1. Whether a pristine 1988 `ULTIMA.EXE` has the same dispatcher return convention. The dispatcher and all
   three loops are in 2001-dated (Upgrade 1.0) files.
2. The exact purpose of TOWN `0x0170`, `0x0c78`, `0x1352`, NPC.OVL `0x0db4` (via thunk 0x7ab2) and K:0x39cc
   (the `0x594f` countdown). The turn gating around them is CONFIRMED; their names are PROBABLE.
3. Whether the DUNGEON key reader `0x3d6` can ever return ≤ −1. If it can, `0x0f3f jle` skips the handler
   with `si` still 1, which makes it a free turn. As read, it loops until it has a nonzero key.
4. How the arrow keys reach getdir as codes 1–4 when `[0x538a]==0`. K:0x266c `0x2712` adds 0xfa to codes
   below 5 in that mode. This does not affect the Space/`Pass` conclusion.
5. Whether any Use item changes `g_location`. Overworld and town would then leave the loop without the turn
   (MAINOUT `0x0c20`, TOWN `0x15a6`). None of the handlers read for this note do; moonstones and shards
   were not traced to the end.
6. Owner policy for a device Cancel at a world getdir on the T-Deck and in Enhanced mode (§6).
