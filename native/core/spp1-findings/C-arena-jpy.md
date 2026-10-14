# SPP1 candidate C: the arena's J, P and Y

Read-only investigation, 2026-10-14, worktree `parity/shared-preservation-1` @ e089f2c5.
Binaries: `original/u5/ultima5/` (Exodus Upgrade 1.0 install; COMBAT/SJOG/CMDS.OVL dated 1996-12-25).
Tools: `re/tools/dis16.py`, `re/tools/thunks.py`, plus a scratch annotator that resolves overlay
near calls with the fitted bases (COMBAT 0xA290, SJOG/CMDS 0xBF80, DUNGEON 0x81D0) and decodes
DATA.OVL strings (file offset = DS + 0x10). Every resolved callee below was checked for the
`55 8b ec` prologue or the thunk record `9a ec 02 2e 07 <ovl> ea <target>`.

Labels: **CONFIRMED** = read in the binary at the cited offsets; **PROBABLE** = follows from
what was read but rests on one unread link (named); **UNKNOWN** = not settled.

---

## 0. Summary

| key | COMBAT handler | real routine | party gate | prompt | RNG | turn |
|---|---|---|---|---|---|---|
| J | 0x097a -> funnel 0x0544 code 1 | SJOG.OVL 0x0d4a (`cmd_jimmy`, the overworld routine) | yes (0x0557) | keys==0 checked BEFORE getdir; getdir | 0 or 1 draw | always ends |
| P | 0x0994 (direct) | CMDS.OVL 0x161a (`cmd_push`) | none | getdir | none | always ends |
| Y | 0x09c0 (direct) | CMDS.OVL 0x1418 (`cmd_yell`) | none | `what?` + getstring(30) | none | always ends |

* All three are real, functional commands in BOTH arena kinds. Native prints `What?` for all
  three on the T-Deck and in Windows Original/Enhanced (`ui_session.cpp:1718`), with no turn.
* J and Y behave identically in an overworld/town arena and a dungeon room. P differs only in
  that a dungeon room checks the room's trigger plates first (COMBAT 0x111a gate).
* The getdir is the kernel's 0x35EC: arrows answer, **Space prints `Pass` and the turn is
  still spent, ESC is ignored** (re/notes/combat-commands.md says ESC is accepted -- that is
  wrong, and the TS port copied it).
* Yell's getstring: **ESC erases the line and keeps reading; only RETURN leaves**; an empty
  RETURN prints `Nothing` and the turn is spent (the TS port's ESC = `Nothing` + turn is wrong).
* The TS core functions (`Combat.playerJimmy/playerPush/playerYell/playerDirCancel`) match the
  binary's semantics; the divergences are in `main.ts`'s key handling and a few edges (§3).

---

## 1. The arena context (applies to all three)

**g_location in every arena is 0xFF** -- CONFIRMED.
* Overworld/town arena: kernel 0x5F86 saves `[0x5893]` to `[0x5894]` (0x5fa8-0x5fab) and writes
  `mov byte [0x5893],0xff` (0x5fb4); restored at 0x6091-0x6094 on exit. (Live witness also in
  re/notes/combat-light-verdict.md §1: 8 reads of 0xFF.)
* Dungeon room: DUNGEON.OVL 0x007e-0x0081 saves to `[0x5894]`, 0x00a8 writes 0xFF, then runs
  COMBAT's main loop through thunk ovl 7 -> 0xae24 = COMBAT 0x0b94 (0x00c4).
* J/P/Y read only `[0x5893]` (never `[0x5894]`), so the dungeon-corridor variants are
  unreachable in a room (Jimmy's `0x21..0x28` gate 0x0d52-0x0d5e -> 0x0c3e is skipped).

**Room flag `[0x58a1]`** -- CONFIRMED (values), used only by Push's plate check:
* overworld/town fight: kernel 0x6340-0x6347 pushes `0,[bp+4],0` -> 0x5F86 stores its first
  argument (`[bp+8]` = 0) at 0x5f8e-0x5f91 -> `[0x58a1]=0`;
* dungeon room: DUNGEON.OVL 0x00bf `mov byte [0x58a1],0x82`;
* dungeon corridor fight: DUNGEON.OVL 0x0c46-0x0c53 pushes `2,[0x5c67],0` -> `[0x58a1]=2`
  (side observation: COMBAT 0x111a also accepts bit 2, so a corridor arena's triggers, if it
  has any, are live too -- UNKNOWN whether any corridor arena carries triggers).

**The player turn loop** (COMBAT 0x063e) -- CONFIRMED.
* 0x0651-0x065c copies the active combatant's x,y (record +6/+7 at 0xBA1A/0xBA1B) into
  `g_party_x/y` (0x5896/0x5897), so every getdir target is `actor + (dx,dy)`: adjacent only.
* 0x0838 `call 0x83dc` = kernel 0x266C (getkey, upper-cases, so `j`==`J`); 0x083e
  `[bp-4]=1` (leave the key loop), 0x0843 `[bp-2]=0` (end the turn).
* Dispatch: J `cmp ax,0x4a` 0x084a -> 0x097a. P via 0x0ade (`cmp 0x52`) / 0x0ae6 `ja` / 0x0ae8
  `sub ax,0x4b` / `jmp cs:[bx-0x5278]` = table at file 0x0af8, entry 5 = 0xac24 -> file 0x0994.
  Y `cmp ax,0x59` 0x0b3a -> 0x09c0.
* Tail 0x0b56: `[bp-2]!=0` -> 0x06f1 (same actor re-prompted, no turn); `[bp-2]==0` -> 0x0b5f
  turn ends (0x0b76 kernel 0x2A28 for a party member, then for any key outside '0'-'6'
  0x0b85 SJOG 0x2012 via thunk 0x7d16, 0x0b88 kernel 0x5910 redraw, 0x0b8b SJOG 0x1ea4 via
  thunk 0x7e66), return to the main loop 0x0c93 which moves to the next combatant.
* Sleep (bit 8, 0x080a) and dragged-under (bit 4, 0x07d7) are handled BEFORE the key read, so
  a disabled actor never types J/P/Y (native `e.disabled()` already covers this).

**Funnel 0x0544** (used by J only) -- CONFIRMED: prints the name (0x054a `call 0x75c0` =
kernel 0x1850 print_string); gate `test [actor+2],0x80` (0x0557) -> not party: `Can't!\n`
DS 0x6d98 and return 1 (re-prompt, no turn); party: dispatch by code, then 0x05b0 `sub ax,ax`
-- the routine's own result is discarded and the turn ends. P and Y do NOT pass this gate.

**The getdir (kernel 0x35EC)** -- CONFIRMED.
* Clears dx/dy `[0x5876]/[0x5878]` (0x35f2-0x35f8), then loops 0x3618 `call 0x266c` and
  leaves ONLY for `0x20` (0x361e) or codes 1-4 (0x3600-0x3616). ESC (0x1b) re-reads: the
  `cmp ax,0x1b` at 0x363b is unreachable.
* 1 = `West\n` (DS 0xa2b6, dx-1) · 2 = `East\n` (0xa2bc, dx+1) · 3 = `North\n` (0xa2a6, dy-1) ·
  4 = `South\n` (0xa2ae, dy+1); returns 1. Space = `Pass\n` (DS 0xa2a0), returns 0. Four
  directions only; no diagonals.
* So the transcript is `Jimmy-North` / `Push-Pass` on one row (the name has no newline).

**The map read (kernel 0x4402)** -- CONFIRMED: with loc>0x7f it returns
`0xAD14 + y*32 + x` (0x4408-0x441c) with **no bounds check**. The arena buffer 0xAD14 holds the
whole 352-byte CBT record (DUNGEON.OVL 0x0061-0x007b loads 0x160 bytes there; COMBAT 0x111a
reads the trigger table from inside it at 0xae1f/0xae27...), so a target off the 11x11 grid
reads record metadata or neighbouring globals (§3, edge E1).

**The pool occupancy probe (kernel 0x368E)** -- CONFIRMED: sweeps slots 1..31 (0x5c62..0x5d5a,
8-byte records), matches `+2==x`, `+3==y`, skips the floor test when loc>0x7f (0x36c0-0x36c3)
and returns the slot's `+0` type byte (0 = free). Side effect: it writes the slot index into
`[0x5876]` (0x36d8/0x36f3) -- harmless for Push, which saved dx first (0x163c). It returns at
the FIRST (x,y) match even if that slot's type is 0 (only matters for cell (0,0): PROBABLE
quirk, not worth porting).

**RNG** -- CONFIRMED: kernel 0x2092 `rand(lo,hi)` (args `push lo; push hi`) =
`lo + (seed&0x7fff) % (hi-lo+1)`, seed `[0x5420]` = `ror3(seed+0x9248)^0x9248 + 0x11`
(0x2098-0x20aa); inclusive both ends; one step per call. SJOG reaches it through `call 0x6112`.

---

## 2. Per command

### 2.1 J -- Jimmy (SJOG.OVL 0x0d4a)

Chain -- CONFIRMED: COMBAT 0x097a `push 0x6e1a` (`Jimmy-`, the arena's own copy) / `push 1` ->
0x0971 funnel -> 0x0586 `call 0xffffdb82` -> CS 0x7e12 thunk (ovl 14 = SJOG, target 0xccca) ->
SJOG 0x0d4a (prologue verified; ovl 14 is SJOG: 0x0d4a is `55 8b ec` in SJOG and not in CMDS).

Order -- CONFIRMED:
1. 0x0d52-0x0d5e: loc in 0x21..0x28 -> dungeon-corridor jimmy 0x0c3e. Never in an arena.
2. 0x0d66: `g_keys` `[0x57ac]==0` -> `No Keys!\n` (DS 0x8ad0), exit. **Before the getdir**:
   no direction is asked. Turn spent (funnel discards). 0 draws.
3. 0x0d78 getdir; 0 (Space) -> exit (turn spent, `Pass` already printed).
4. Target `(x+dx, y+dy)`; 0x0d9e kernel 0x4402 -> tile T.
5. Dispatch on T (0x0daa-0x0dc4, 0x0f1c-0x0f29):

| T | path | draws | success | failure |
|---|---|---|---|---|
| 0xB9, 0xBB (locked door / windowed) | 0x0dc8 | `rand(0,29)` 0x0dd4-0x0ddb | DEX > roll (0x0deb `ja`): map T-1 (0xB8/0xBA) 0x0e04-0x0e0e, `[0x24e6]|=2`, `Unlocked!\n` DS 0x8ae6; key kept | `Key broke!\n` DS 0x8ada, `dec [0x57ac]` 0x0df6 |
| 0x97, 0x98 (magic lock) | 0x0e1c | 0 | -- | always `Key broke!\n` DS 0x8af2 + key |
| 0x84, 0x85 (stocks/manacles) | 0x0e22 (occupant check 0x0e29-0x0e3f skipped, loc>=0x80) | `rand(0,29)` 0x0e54-0x0e5b | DEX > roll; loc>=0x7f (0x0e76) -> 0x0ee4: map = 0x44, `[0x24e6]|=2`, `Unlocked\n` DS 0x8b48 (**no '!'**); no NPC freed, **no karma** | `Key broke!\n` DS 0x8b10 + key |
| anything else | 0x0f2c pool sweep for a chest | see below | | |

   Chest sweep 0x0f2c-0x0f6f: slots 1..31 ascending, `+2==x`, `+3==y`, floor test skipped
   (0x0f56 loc>0x7f), `+0==1` (chest). None -> `No lock!\n` DS 0x8b52 (0 draws, key kept).
   Found -> 0x0baa(slot):
   * 0x0bc0 lock byte = slot `+5` (0x5c5f+slot*8). `<0x80` (untrapped) -> `Key broke!\n`
     DS 0x8a58 + tone + key, **0 draws** (0x0bcb -> 0x0c1d).
   * trapped: threshold = `byte(((b&0x7f) - DEX + 30) >> 1)` -- 16-bit subtract, unsigned
     `shr`, truncated to a byte (0x0bd2-0x0bee). `rand(1,30)` (0x0bf1-0x0bf9). roll <=
     threshold -> `Key broke!\n` DS 0x8a6e + tone + key (0x0c1a); roll > threshold ->
     `Success!\n` DS 0x8a64 and `and [slot+5],0x7f` (trap disarmed; chest stays shut).
     Byte-wrap: when DEX > (b&0x7f)+30 the threshold becomes 0xE2..0xFF -> always fails.
   * The tone: kernel 0x43AE(0x32,1,0x7d0,0x320) at 0x0c31, on both chest `Key broke!` paths
     only (door/magic/manacle failures go 0x0df2 with no tone). PROBABLE it is the A3 tone
     sweep (0x43ae calls 0x22e2 in a loop); not ported anywhere in native.
6. DEX source -- CONFIRMED: kernel 0x4988 with loc>0x80 (0x4995-0x49a6) returns combatant
   `+3` = roster index, no `Player:` prompt; DEX = roster byte 0x55b5+idx*32 (the roster
   record's DEX, not a combat cache).

Probabilities: door/manacle success = min(DEX,30)/30; trapped chest success =
(30 - threshold)/30 when threshold<30.

### 2.2 P -- Push (CMDS.OVL 0x161a)

Chain -- CONFIRMED: COMBAT 0x0994 prints DS 0x6e28 `Push-`, 0x099b `call 0xffffda7a` -> CS
0x7d0a thunk (ovl 15 = CMDS, target 0xd59a) -> CMDS 0x161a; 0x099e `jmp 0x7ba` -> turn ends
(`[bp-2]` still 0). No party gate.

Body -- CONFIRMED:
1. 0x162f kernel 0x39CC (town restore-tile): inert, loc 0xFF fails `1..0x20` (0x39cf-0x39db).
2. 0x1632 getdir; Space -> silent exit (turn spent, `Pass` printed by getdir).
3. loc>0x7f: saves g_party, loads the actor's cell (0x164f-0x1671).
4. Target (tx,ty) = actor + d. loc>0x7f: 0x1699 thunk 0x7eba -> COMBAT 0x111a(tx,ty): only if
   `[0x58a1]&0x80 || &2` (0x1127-0x1135); walks the 8 trigger slots, every live match (`at`
   == target) is fired (two tiles written when their coordinates are < 11, `at` set to 0xFF)
   and returns 1 -> Push exits **silently, nothing moves, turn spent** (0x169e).
5. T = map[target]. Occupied (0x16bf kernel 0x368E: any pool object -- combatant, chest,
   corpse/blood, loot, field) OR not pushable (0x16c9 CMDS 0x14ba) -> `Won't budge!\n`
   DS 0x4559.
   Pushable (0x14ba): 0x5B, 0x90-0x93, 0xA5-0xA6, 0xA8-0xA9, 0xAD-0xAF, 0xB4-0xB7.
6. fill = 0x45 if (T&0xFC)==0xB4 else 0x44 (0x16da-0x16eb). B = map[target+d], A =
   map[actor].
7. beyond free (0x172e 0x368E==0) AND B==fill -> 0x1548 PUSH: `Pushed!\n` DS 0x4547;
   map[beyond]=T; map[target]=B; if class (T&0xFC) is 0x90 or 0xB4, map[beyond] =
   class + idx with idx N0 E1 S2 W3 (0x1504).
   else A==fill -> 0x15b0 PULL: `Pulled!\n` DS 0x4550; map[actor]=T; map[target]=A; class
   0x90/0xB4 -> class + (idx^2).
   else `Won't budge\n` DS 0x4567 (**no '!'** -- a different string).
8. After a push or pull (0x178a-0x17e3): the actor moves +d by raw addition -- g_party,
   combatant +6/+7, and a pool slot (see U1) -- then kernel 0x5910 redraw. It does not go
   through the mover (SJOG 0x1d3c), so a plate under the vacated cell is **not** fired.
   `[0x24e6]=1` (0x1798). This is not observable on its own: a live plate on the target cell
   has already been fired by step 4, which ends the command before anything moves.
9. Zero RNG on every path.

U1 -- UNKNOWN: 0x17af-0x17d3 updates the pool slot indexed by combatant `+3` (roster index),
whereas COMBAT's mover uses `+4` (0x0f5d-0x0f8f) and the kernel's placement writes `+4`
(0x6a7f, 0x6aac-0x6afd). If `+3 != +4` for some party member, the binary moves a different
pool object. Irrelevant to native (one `CombatActor.position`), noted for completeness.

### 2.3 Y -- Yell (CMDS.OVL 0x1418)

Chain -- CONFIRMED: COMBAT 0x09c0 prints DS 0x6e4e `Yell ` (arena copy), 0x09c7
`call 0xffffdada` -> CS 0x7d6a thunk (ovl 15, target 0xd398) -> CMDS 0x1418; 0x09ca
`jmp 0x7ba` -> turn ends. No party gate.

Body -- CONFIRMED:
* Sails branch 0x1423-0x1456 needs transport `&0xF8==0x20` AND loc<0x80 (0x142c): with
  loc 0xFF it is unreachable, **even in a ship fight** -- no `FURL!`/`HOIST!` in an arena.
* 0x1458 `what?\n:` (DS 0x4529) -> rows `Yell what?` / `:`; 0x1467 getstring(buf,30) =
  kernel 0x3B1C: printable 0x20-0x7f echoed (0x16ba) up to 30; Backspace/left arrow delete;
  **ESC erases the whole buffer and keeps reading** (0x3b4e-0x3b5d); only RETURN leaves
  (0x3b7c). Getkey upper-cases.
* empty -> `Nothing\n` (DS 0x4531) on the `:` row (no newline before it).
* a word -> 0x147e putchar('\n') (kernel 0x16BA), then loc 0xFF is neither 1..0x20
  (0x1481-0x148d) nor 0 (0x149c) -> `\nNo effect!\n` (DS 0x453a). Rows: `:WORD`, blank,
  `No effect!`. Every word (Words of Power, mantras) has no effect in an arena.
* Zero RNG; no state change other than the turn.

### 2.4 Overworld/town arena vs dungeon room

| | overworld/town arena | dungeon room |
|---|---|---|
| J | same code; BRIT.CBT has no lock tiles (census below), so in practice chests or `No lock!` | same code; locked doors and manacles exist |
| P | trigger check skipped (`[0x58a1]=0`) | live plates fire silently |
| Y | `No effect!` / `Nothing` | same |

Tile census of the 11x11 tile area of every CBT record (script, CONFIRMED):
* BRIT.CBT (16 maps): no 0x84/0x85/0x97/0x98/0xB8-0xBB; pushables 0xA6 x7 and 0xAF x1 (map 9);
  0x4E x1 (map 9).
* DUNGEON.CBT (112 records): 0xB9 rooms 71, 93; 0xBB rooms 6, 33, 36-40, 63, 66; 0x97 room 9;
  0x98 rooms 33, 81, 84; 0x85 rooms 38, 40; pushables 0x5B (47), 0x90 (32, 84, 111), 0x92
  (47, 84, 111), 0xA6 (32, 84, 86), 0xAF (37, 38, 111).

---

## 3. The TS reference (game/src/core/combat/combat.ts, game/src/main.ts)

Core semantics -- CONFIRMED match: `playerJimmy` (combat.ts:4340) gate order, the four tile
classes, chest disarm with the byte-wrapped threshold (`jimmyLock`, world/commands.ts:193-235),
key spent only on failure, roster DEX, combat RNG stream; `playerPush` (combat.ts:4544) plate
-> occupancy -> pushable -> push/pull/`Won't budge` with the two different strings, class
reorientation (`pushOrientedTile`, commands.ts:147), raw actor advance without triggers;
`playerYell` (combat.ts:4614) `Nothing` / `\nNo effect!\n`, turn spent.

Divergences:
* **D1 (UI, main.ts:3076-3091)**: ESC in the J/S/P getdir prints `Pass` and spends the turn.
  Binary: ESC is ignored (0x3618-0x3620). Only Space is `Pass`. Root cause: the note
  `re/notes/combat-commands.md` (§TURNO) reads the dead `cmp ax,0x1b` at 0x363b. (The TS G/O
  getdir -- ESC silent and free, Space ignored -- is wrong the other way; native Original
  already does it right in `dos_getdir_pass`.)
* **D2 (UI, main.ts:3537-3540)**: Yell's ESC calls `playerYell("")` = `Nothing` + turn.
  Binary: ESC erases and keeps reading; only RETURN leaves.
* **D3 (edge)**: TS `tileAt` returns -1 off the grid (combat.ts:1826-1829) -> `No lock!` /
  `Won't budge!`. Binary reads raw buffer bytes (kernel 0x4402 has no bounds check). Two
  DUNGEON.CBT records (65, 90) hold 0x98 at column 11 of row 5, so jimmying east from (10,5)
  there gives `Key broke!` and costs a key in the binary (PROBABLE: assumes the in-RAM
  record equals the file). North/south/west off-grid reads are UNKNOWN content.
* **D4**: the chest `Key broke!` tone (SJOG 0x0c31) is not ported (TS records it as #161).
* **D5 (edge)**: TS prefers a board chest over a nested pile chest in one cell; the binary
  takes the lowest pool slot with type 1. Nested chests carry `+5` = qty <= 0x7f per the TS
  (PROBABLE, not re-read here), so Jimmy on one is always `Key broke!` with 0 draws.

No generator drives these functions: `game/tests/combat-jspy-121.test.ts` (25 tests) is the
only consumer.

---

## 4. Native today

### 4.1 What a J/P/Y keypress does in an arena -- CONFIRMED

* **T-Deck**: the matrix sends `j` (col 3,row 6), `p` (col 1,row 3), `y` (col 3,row 2)
  (`keyboard_matrix.cpp:9-15`; Shift upper-cases, :49). `UiInputAdapter::translate` makes a
  `UiActionKind::Character` (`ui_input_adapter.cpp:144-149`); Movement Mode only rewrites
  w/a/s/d (:134-143); Alt chords are M/D/S/L (:119-133). `AlphaRuntime` passes it to
  `ui_->handle_input` (`alpha_runtime.cpp:2431`; queued first if the AI is acting, :2428-2430).
  `UiSession::handle_combat` (`ui_session.cpp:1630`) has no `j`/`p`/`y` arm -> default
  `append(UiTextChannel::Combat,"What?")` (`ui_session.cpp:1718`): no command, no turn.
* **Windows Original**: `j`/`p`/`y` arrive as SDL text -> Character (`desktop_input.cpp:219-266`).
  The DOS refusal table (`ui_session.cpp:1652-1670`) does not list them, so they fall through to
  the same `What?` (:1718). Recorded as Δ in `ORIGINAL_MODE_FIDELITY.md` §3.14 and §7 item 3.
* **Windows Enhanced**: same path as the T-Deck -> `What?`.
* **Core**: even if sent, `CommandKind::Jimmy/Push/Yell` in combat are rejected:
  `commands.cpp:811` `(c.combat&&!ready_anywhere)` -> `InvalidContext`. There is no
  `CombatAction` for them (`combat.h:164-184`; `combat.cpp:1367` rejects ordinals > `SetActive`).

### 4.2 Are j/p/y (or their symbol twins) already used in combat? -- CONFIRMED no

`handle_combat` binds a, c, g, k, o, s, r, u, space, 0-6 (and in Original b, e, f, h, i, l, m,
n, q, v, x, t, d, w, z) -- `ui_session.cpp:1652-1716`. The T-Deck symbol layer sends `;`, `@`,
`)` for those three keys (`keyboard_matrix.cpp:17-23`); none has a combat arm either. No
desktop shortcut uses J/P/Y (`desktop_input.cpp:122-214`). Nothing needs repurposing.

### 4.3 What already exists to build on

* Arena Get/Open/Search: `combat.cpp:1485-1638` (direction ordinal 0=E 1=W 2=S 3=N, adjacent
  target, `e.message(...)`, `e.advance()`); `unopened_chest()` and the lock byte
  `(chest_contents&127)|(loot==129?128:0)` (:1626) are exactly Jimmy's `+5` byte.
* Room plates: `Engine::triggers(x,y)` (`combat.cpp:172-184`) = COMBAT 0x111a (room gate, two
  tiles, `at={255,255}`).
* Occupancy pieces: `Engine::occupant()` (active actors, :121-128), `loot[]` codes 1/129
  chest, 30 corpse, 31 blood (:243, :516-519), `piles`, `fields`.
* World twins: `world_interaction` Jimmy/Push (`world_commands.cpp:73-109`) -- same tables,
  pushable set, fill and facing; the arena versions differ only in the sources of tiles,
  occupancy and DEX.
* UI: `begin_target(UiRequestId::Direction,...)` for arena getdirs (`ui_session.cpp:1690-1699`),
  the arena target-marker branch (:1212-1219), `getdir_prints_word` (:56-62), Original's
  `dos_getdir_pass` (Space = `Pass` + `CombatYield` for arena kinds, :1375-1392, ESC swallowed
  at :1046), Yell's `YellText` modal (:1427, :1465-1468, :614-619, :682-685).
* Owner mirrors: `set_sail_context` (`ui_session.h:311`, refreshed in
  `AlphaRuntime::refresh_session_context`, `alpha_runtime.cpp:1842-1845`).

### 4.4 Fixtures and generators that would observe a new action

* `native/core/tools/generate-combat-fixtures.ts` -> `fixtures/combat.txt` and the `--negate`
  corpus `fixtures/combat-negate.txt`, read by `combat_parity_test.cpp` which maps
  `op -> CommandKind(int(CombatMove)+op)` (:354; ops 0-7, 8 peek, 9/10 handoff). Neither
  scripts J/P/Y; its `snap()` has no `keys` field. Appending enum values shifts nothing.
* `--real-arenas` (same generator, `build-real-arenas/`, `OPENU5_REAL_ARENAS`): the real CBT
  maps (doors, manacles, pushables, room triggers) -- the natural place for a J/P sweep.
* `generate-advanced-combat-fixtures.ts` (`advanced-combat.txt`) and
  `generate-command-fixtures.ts` (`commands.txt`, CommandKind ordinals): unaffected by appended
  values; neither drives J/P/Y.
* `game/tests/combat-jspy-121.test.ts`: the TS semantics oracle (expected strings, draws).
* Windows scenario `native/targets/windows/scenarios/original_arena.txt` (Original keys, `expect
  console ...`, `combat_actions_advanced`) -- will see the change once routed.

---

## 5. Recommended implementation

### 5.1 Core (append only)

* `CombatAction`: append `Jimmy, Push, Yell` after `SetActive` (`combat.h:183`); move the
  `combat.cpp:1367` guard to the new last value.
* `CommandKind`: append `CombatJimmy, CombatPush, CombatYell` after `CampAcknowledge`
  (`commands.h:41`). (Alternative: reuse `Jimmy/Push/Yell` with `c.combat`, the SetActive
  precedent; new kinds keep UiSession's arena lists uniform and are recommended.)
* `commands.cpp:671-673`: add the three kinds to the combat arm; map them at :700-706; convert
  `cmd.direction` to the 0..3 ordinal like Get/Search (:711-713), `-1` when `!has_direction`;
  for `CombatYell` pass `x = cmd.text_length ? 1 : 0`.
* `combat_action`, inside the switch after `e.disabled()` (model on `Search`, :1619-1638):
  * **Jimmy**: `g.keys<=0` -> `No Keys!` + advance (0 draws) -- checked before the direction.
    `x<0` -> advance only. Target off-grid -> `No lock!` (record D3). T=0xB9/0xBB ->
    `e.rand(0,29)`, `dex>roll` ? tile-1 + `Unlocked!` : `Key broke!` + `keys--`. 0x97/0x98 ->
    `Key broke!` + key, 0 draws. 0x84/0x85 -> `e.rand(0,29)`, success tile 0x44 + `Unlocked`
    (no '!'), else `Key broke!` + key. Else `unopened_chest(key)`: lock byte as Search;
    `<0x80` -> `Key broke!` + key, 0 draws; else `threshold=(uint16_t((b&127)-dex+30)>>1)&255`,
    `e.rand(1,30)<=threshold` ? `Key broke!` + key : `Success!` + `loot[key]=1`. Else a pile
    with id 1 at the cell -> `Key broke!` + key, 0 draws (D5). Else `No lock!`. DEX =
    `g.party.characters[a->member].dexterity`. Always `e.advance()`. No karma.
  * **Push**: `x<0` -> advance. `s.room` and a live trigger at the target -> `triggers(tx,ty)`,
    no message, no move, advance. Occupied(target) or not pushable -> `Won't budge!`.
    occupied() = `occupant()` || `loot[cell]>0` || a pile at the cell || a field at the cell.
    Push / pull / `Won't budge` exactly as §2.2 (reuse the world function's pushable set, fill
    and facing). On push/pull move the actor by assignment plus a `Moved` event -- **not**
    `e.move()` (no trigger on entry). 0 draws. Advance.
  * **Yell**: `x==0` -> `Nothing`; else `No effect!` (decide the `\n` framing against how
    Combat messages render); 0 draws; advance.
* Optional: a presentation-derived SFX for the chest `Key broke!` tone (D4), keyed on the
  core's literal, per the existing audio rule.

### 5.2 UiSession / runtime routing

* `handle_combat` (`ui_session.cpp:1686-1718`): `j` -> `command_echo("Jimmy-")`, then if the
  mirrored key count is 0 dispatch `CombatJimmy` without a direction (core prints `No Keys!`,
  turn ends), else `begin_target(Direction,...)`; `p` -> `command_echo("Push-")` +
  `begin_target`; `y` -> Original: `command_echo("Yell what?")` + `begin_text(YellText,":",30)`;
  T-Deck/Enhanced: `command_echo("Yell")` + `begin_text(YellText,"Yell what?",15)`.
* Key mirror: add a `set_combat_keys(bool)` (or extend an existing owner mirror) fed from
  `refresh_session_context`; the core re-checks keys authoritatively (R-19 pattern).
* Add `CombatJimmy`/`CombatPush` to `getdir_prints_word` (:56-62), the arena target-marker
  branch (:1212-1214) and `dos_getdir_pass`'s `arena` list (:1384-1386).
* `finish_modal` YellText (:614-619, :682-685): when the modal returns to Combat, dispatch
  `CombatYell` (with the text). In Original the empty case must still dispatch (today it
  returns with no command, which in an arena would leave the turn unspent); keep `Nothing` on
  the `:` row by letting the UI print it and flagging the command as already echoed (the
  `typed_name`/`item_named` precedent), so the core only advances the turn.
* Cancel policy: Original follows the binary (ESC ignored in getdir, ESC erases in Yell, Space
  = `Pass` + turn) -- already what the shared modal code does. T-Deck/Enhanced: keep the
  existing handheld contract of G/O/S (Cancel leaves the prompt with no turn) -- an explicit
  decision for the owner, recorded as a device convention, not a parity claim.
* Input bindings: none to add -- `j`/`p`/`y` already reach `handle_combat` as Characters on
  both targets and nothing else uses them in combat (§4.2).
* Update `ORIGINAL_MODE_FIDELITY.md` §3.14 and §7 item 3 when landed; correct
  `re/notes/combat-commands.md` §TURNO's ESC claim.

### 5.3 RED-first test plan

**Core** (new test modeled on `combat_loot_open_regression_test.cpp` / the
`batch21a3_combat_active_player_test.cpp` fixture; one party actor at (5,5), member 0, a foe
far away, a trace sink counting draws, turn advance checked as the scheduled actor or
`action_count` changing against a primed baseline). All RED today (the action does not exist).

| id | setup | expect |
|---|---|---|
| J1 | keys 0 | text `No Keys!`; keys 0; 0 draws; turn advanced; no direction needed |
| J2 | 0xB9 east, DEX 30, keys 2 | `Unlocked!`; tile(6,5)=0xB8; keys 2; draws == [(0,29,v)] |
| J3 | 0xBB east, DEX 0 | `Key broke!`; tile 0xBB; keys 1; 1 draw (0,29) |
| J4 | 0x97 | `Key broke!`; keys-1; 0 draws |
| J5 | 0x85, DEX 30 | `Unlocked` (no '!'); tile 0x44; 1 draw; karma unchanged |
| J6 | chest loot=1 | `Key broke!`; keys-1; 0 draws; loot still 1 |
| J7 | chest loot=129, contents 0, DEX 30 | `Success!`; loot 1; contents 0; keys kept; 1 draw (1,30) |
| J8 | chest loot=129, contents 0, DEX 50 | threshold 0xF6: `Key broke!` for every seed; trap kept; 1 draw |
| J9 | floor 0x44 | `No lock!`; keys kept; 0 draws; turn advanced |
| J10 | x=-1 (Space after `Pass`), keys>0 | no text; 0 draws; turn advanced |
| J11 | actor (10,5), east | `No lock!` (D3 recorded) |
| P1 | 0xB4 at (6,5), 0x45 at (7,5) | `Pushed!`; (7,5)=0xB5; (6,5)=0x45; actor (6,5) + Moved; 0 draws |
| P2 | 0x5B at (6,5), wall behind, actor on 0x44 | `Pulled!`; (5,5)=0x5B; (6,5)=0x44; actor (6,5) |
| P3 | pull 0xB4 east | (5,5)=0xB7 |
| P4 | foe on (6,5) | `Won't budge!`; nothing moves |
| P5 | 0x44 target | `Won't budge!` |
| P6 | 0x5B at (6,5), 0x45 at (7,5), actor on 0x45 | `Won't budge` (no '!'): fill 0x44 matches neither |
| P7 | 0xB4 with 0x44 behind | no slide (fill is 0x45) |
| P8 | loot[(7,5)]=30 behind a 0x5B, actor on 0x44 | pull, not push |
| P9 | room, live trigger at (6,5) | trigger tiles written, `at`=255; no text; actor stays; turn advanced |
| P10 | not room, same trigger data | trigger untouched; normal push rules |
| P11 | push succeeds; spy on `triggers()` | the actor's advance is an assignment + `Moved`, never `e.move()` (guards the raw move even though §2.2 step 8 makes it unobservable in the binary) |
| Y1 | x=0 | `Nothing`; 0 draws; turn advanced |
| Y2 | x=1 | `No effect!`; 0 draws; turn advanced |

**UiSession** (`win_dos_interface_test.cpp` style, both modes, in Combat mode):
* Original: `j` -> `Jimmy-`, TargetSelection; ESC/RETURN/Backspace no answer; Space ->
  `Jimmy-Pass` + `CombatYield`; arrow -> `Jimmy-North` + `CombatJimmy` with direction. Keys
  mirror 0 -> `CombatJimmy` with no direction, no prompt. `p` likewise (`Push-Pass`). `y` ->
  `Yell what?` / `:`; ESC erases; empty RETURN -> `:Nothing` and a turn-ending dispatch;
  `ORT` -> `CombatYell` text `ORT`.
* T-Deck/Enhanced: `j`/`p` open `Direction?`; Cancel leaves with no command (documented); `y`
  opens `Yell what?`. Assert `j`/`p`/`y` no longer print `What?` (RED today: they do).

**Device/scenario**: a T-Deck host test on the real `AlphaRuntime` (raw keys, model
`a3_hf1_arena_loot_test.cpp`) for one J door in a dungeon room and one P pull; extend
`scenarios/original_arena.txt` with `text j` / `key Space` / `expect console Jimmy-Pass` /
`expect combat_actions_advanced 1`, and `text y`, `key Return`, `expect console :Nothing`.

**Parity corpus**: add a `--jpy` corpus to `generate-combat-fixtures.ts` (new file
`fixtures/combat-jpy.txt`, like `--negate`, so `combat.txt` stays byte-identical) that scripts
`playerJimmy/playerPush/playerYell/playerDirCancel` on the real CBT maps with `keys` and piles
added to the snapshot, and map new ops (e.g. 11/12/13) in `combat_parity_test.cpp`. The
generator must not script ESC as a cancel (D1/D2 are UI-layer divergences, not core ones).
Mutation targets: the keys-before-getdir order, `>` vs `>=` on DEX, the byte truncation of the
chest threshold, `Won't budge!` vs `Won't budge`, the XOR-2 pull orientation, the fill class,
the occupancy check on the beyond cell, and the no-trigger raw move.
