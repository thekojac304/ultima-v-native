# SPP1 Candidate B — shop offers and transactions (binary vs native)

Investigator: read-only RE pass, 2026-10-14. Worktree `C:\Dev\ultima-v-native-parity`
(branch `parity/shared-preservation-1`). No tracked file was changed.

## 0. Method, provenance, conventions

- All DOS claims below come from my own disassembly of `original/u5/ultima5/SHOPPES.OVL`,
  `SHOPPES2.OVL`, `SHOPPES3.OVL`, `TALK.OVL`, `MAINOUT.OVL`, `ZSTATS.OVL`, `CAST2.OVL`
  and `ULTIMA.EXE` (`--exe`), with a scratch annotator that rebases near calls
  (SHOPPES 0xA290; SHOPPES2/3, ZSTATS, CAST2 0xE1E0; TALK 0xBF80; MAINOUT 0x81D0) and
  resolves overlay thunks with `re/tools/thunks.py` (thunk offsets re-based on the fitted
  bases, not the tool's `min()` bases). Every resolved call was checked to land on
  `55 8b ec`. DS strings are read from DATA.OVL at `DS+0x10`. SHOPPE.DAT records are
  identified by their compressed offset (record n starts after the n-th NUL) and quoted
  from `game/assets/shoppe.json`; "record N" below = shoppe.json[N].
- Offsets: `S1:` = SHOPPES.OVL file offset, `S2:` = SHOPPES2.OVL, `S3:` = SHOPPES3.OVL,
  `TALK:`, `K:` = ULTIMA.EXE kernel CS offset.
- **Provenance caveat.** This is the "Exodus Upgrade 1.0" install: SHOPPES*.OVL/TALK.OVL
  are dated 1996, ULTIMA.EXE 2001 and **DATA.OVL 2020-12-15** (patched). All strings and
  tables quoted come from that DATA.OVL. I did not compare against a pristine 1988 copy.
- Kernel routines identified for this work (new names marked *): `K:0x1850` print_string,
  `K:0x16BA` emit_char, `K:0x266C` getkey (upper-cases a..z via `K:0x2032`), `K:0x2092`
  rand_range(lo,hi) inclusive, `K:0x3B9E` getnum (max N chars incl. a leading `+`/`-`,
  ESC erases, only RETURN ends, `-` negates), `K:0x3B1C` input_string, `K:0x2E8E` roster
  member picker, `K:0x0442` 32-bit mul, **`K:0x03A0` UNSIGNED 32-bit divide*** ,
  **`K:0x0496` SIGNED 32-bit divide***, `K:0x3EF0` add_byte_capped(ptr,n,cap),
  `K:0x3F14` add_word_capped(ptr,n,cap), `K:0x3F36` sub_byte_floored, `K:0x3F54`
  sub_word_floored, `K:0x1F12` cursor_x*, `K:0x1CEE` cursor_y*, `K:0x1BF2` set_cursor*,
  `K:0x1B94` select_window*, `K:0x1C22` define_window*, `K:0x2900` draw_status_panel,
  `K:0x2884` draw_gold*, `K:0x39FC` first_conscious_member*, `K:0x4F7C` advance_minutes,
  `K:0x400C` ring-of-regeneration tick*, `K:0x3AE6` flash+beep.
  Shop-internal: `S1:0x0026` template expander (`$` keeper `[0xaafe]`, `#` shop
  `[0xaafc]`, `@` morning/afternoon/evening by `g_hour` <12/<18, `%` = `[0xb118]` price,
  `^` = `[0xb11a]` qty, `&` = `[0xab00]` item, `*` = `[0xac62]` place, bytes >=0x80 =
  dictionary words), `S1:0x017A` print SHOPPE.DAT record (expanded), `S1:0x019A`
  Falsehood drain, `S1:0x01B6` greeting, `S1:0x0202` farewell, `S1:0x0280` strict Y/N.
  SHOPPES2/3 reach these through thunks `0x7FB6`(0x017A) `0x7FDA`(0x019A) `0x7FE6`
  (0x01B6) `0x7FF2`(0x0202) `0x7F6E`(0x0026) `0x7F62`(0x0280).

Native files read: `native/core/src/shops.cpp`, `native/core/src/shop_orchestration.cpp`,
`native/core/include/openu5/shops.h`, `shop_orchestration.h`, `shop_tables.inc`,
`shop_greetings.inc`, `native/core/src/ui_session.cpp`,
`native/targets/tdeck/main/alpha_runtime.cpp` (shared by the Windows build).

---

## 1. Shared mechanics (all shop types)

### 1.1 Dispatcher — TALK.OVL 0x00E6 (CONFIRMED)

1. `TALK:0x00ED-0x0104`: if `(g_transport_tile[0x587C] & 0xFE) == 0x12` and type != 0x83
   → `"A merchant says:\n\"GET THAT HORSE OUT OF HERE!\"\n"` (DS 0x9072), no shop.
2. `TALK:0x0108-0x011C` **buyer** `charIdx` = active player `[0x587B]` if != 0xFF, else
   `K:0x39FC` = first party member whose status is `G` or `P` (sleepers and the dead are
   skipped). This `charIdx` is pushed to every shop entry point and supplies the INT used
   for haggling and the gender used for "milady/sir/m'lady/m'lord".
3. `[0xB116]` = type (dialog code − 0x81), `[0xB114]` = index of `g_location` in the
   type's town list (DS 0x23CA, stride 16), shop name `[0x21CA+type*32+idx*2]`, keeper
   `[0x22CA+…]`.
4. Jump table: 0 Blacksmith `S1:0x12B2`, 1 Barkeeper `S2:0x066C`, 2 Horse `S1:0x07BE`,
   3 Shipwright `S2:0x0ABC`, 4 MagicSeller `S1:0x075E`, 5 Guild `S1:0x04A2`,
   6 Healer `S1:0x14F8`, 7 Inn `S3:0x08B4`.

### 1.2 Haggle formula and its division

`price = base + (base*(100-3*INT))/100`, two steps, the inner quotient added to a 16-bit
word. The 32-bit product is built as a proper signed value (`sub/sbb/neg/adc/neg`), but:

- **SHOPPES and SHOPPES2 divide with `K:0x03A0`, an UNSIGNED 32-bit divide**
  (guild `S1:0x0315`, reagents `S1:0x05A8`, horse `S1:0x08B7`, blacksmith buy
  `S1:0x09F9`, rations `S2:0x03E7`, frigate `S2:0x097B`, skiff `S2:0x0A16`).
- **SHOPPES3 (inn) divides with `K:0x0496`, SIGNED** (`S3:0x00CE`, `0x03A6`, `0x0778`).
- Consequence: for INT <= 33 the factor is >= 1 and both agree with native. For
  **INT >= 34** the negative product is divided as an unsigned 32-bit number; the low word
  of the quotient is ~0x5C28, so every SHOPPES/SHOPPES2 price becomes **~23,600-25,600**
  (e.g. base 100 → 23,690 at INT 34, 23,642 at INT 50), always more than the 9,999 gold
  cap → every purchase is refused. The inn alone gives the "discount" the notes describe.
- Native `shop_buy_price` (`shops.cpp:59-61`) is signed everywhere; the corpus pins it
  (`native/core/build-shops/helpers.txt`: `0 100 34 98 …`), and `re/notes/shops.md:37`
  ("INT 50 ⇒ ~½base") is wrong for these overlays.
- Sell price `S1:0x0EAC-0x0ED6`: `mul word base` then **`sub dx,dx`** — `INT*base` is
  truncated to 16 bits before `*3/100+1`. Native `shops.cpp:62-64` keeps 64 bits. Differs
  only when `INT*base > 65535` (INT >= 33 at the 2000-gold item).
- Reachability: native caps INT at 30 (shrine `quest.cpp:163`, Developer row
  `ui_debug_menu.cpp:129`), initial companions are <= 24, so both are **latent**
  (out-of-range saves / PC-save imports only).

### 1.3 Falsehood drain `S1:0x019A` (CONFIRMED)

`if g_shadowlord_here_idx[0x5958]==0: gold -= rand(1,64)` (floored). Called after the
payment in: guild, reagents, horse, blacksmith buy, healer pay prompt, tavern round and
house drink, wine, rations (full purchase only, not the "afford only N" path),
**shipwright `S2:0x0846`**, inn rest `S3:0x0121`, inn pickup `S3:0x07B3`. Not called by:
blacksmith sell, rumors, inn leave, free healing (Minoc / Skara Brae charity).

### 1.4 Greeting / farewell / Y-N helpers

- Greeting `S1:0x01B6`: `'"'` + record `[0x3B2A + type*8 + rand(0,3)*2]` + then, by
  `cursor_x`: `>11` → DS 0x784C `"\n\n:"`, `==0` → DS 0x7850 `"\n:"`, else `' '`.
- Farewell `S1:0x0202(k)`: k=0 → DS 0x7854 `"\n\n\""` + record `[0x3B6A+…+rand(0,3)]`
  (no purchase); k=1 → DS 0x7858 + `[0x3BAA+…]` (purchase); then `'\n'` if `cursor_x!=0`
  and expand DS 0x785C `"says $.\n"`. **Any other k (0xFFFF, -2) prints nothing and draws
  no rand.**
- `S1:0x0280`: loop until Y or N, echo DS 0x786A `Yes` / 0x7866 `No`. All in-shop deal
  questions are of this strict kind (they re-read RETURN/ESC/Space).

---

## 2. Divergence summary

(a) = semantic (gold / inventory / state / RNG / flow), (b) = presentation-only.

| # | Shop | Divergence | Class | Confidence |
|---|---|---|---|---|
| S1 | all except inn & healer | buyer INT/gender: DOS = active player else first `G`/`P` member; native always `characters[0]` (`shop_orchestration.cpp:63-65`, `:1066`) | (a) price | CONFIRMED |
| S2 | all SHOPPES/SHOPPES2 buys | unsigned haggle division, INT >= 34 → ~23.6k prices (refusal) | (a) latent | CONFIRMED |
| S3 | blacksmith sell | 16-bit truncation of INT*base | (a) latent | CONFIRMED |
| S4 | shipwright | **no Falsehood drain on ship purchase** (`shop_orchestration.cpp:467-487`) | (a) gold + RNG | CONFIRMED |
| S5 | shipwright | `g_ship_flags[0x6605]` state machine missing: 2nd frigate in one visit = "dock space" → OUT; skiff after frigate = +1 skiff on board, no dock object; 2nd skiff without frigate = refused, unpaid; delivery happens on overworld entry (`MAINOUT:0x0D22`) | (a) state | CONFIRMED |
| S6 | inn | time spell cleared on entry (`S3:0x08E0-0x08E5`: `[0x588E]=[0x587A]=0`); native keeps it, so a rest under An Tym ('T') never advances the clock (1000-step guard) and Quickness changes the regen roll count | (a) state/time/RNG | CONFIRMED |
| S7 | inn | no farewell (no `rand(0,3)`) after a successful rest, after "Lord British is missing" (party of 1 Leave), after "morgue" (dead companion), after pickup "GUARDS!"; native `end()` prints one and draws | (a) RNG + (b) text | CONFIRMED |
| S8 | inn | pickup failure calls the guards (`S3:0x07A0` → thunk 0x7AD6 → TOWN.OVL 0x0958 town alarm); native just leaves | (a) NPC state + RNG | CONFIRMED |
| S9 | inn | pickup sets **HP = maxHP** (`S3:0x0864-0x0869`) and skips MP when the companion dies; native skips the HP restore and restores MP even for the dead (`shops.cpp:318-325`) | (a) | CONFIRMED |
| S10 | inn | pickup does not clear months-at-inn (+0x17); native zeroes it (`shops.cpp:312`) | (a) save byte | CONFIRMED |
| S11 | inn | Leave marks the visit as a purchase (`S3:0x03D8`, farewell table 1); native does not mark (`shop_orchestration.cpp:488-491`) | (b) farewell text (same rand) | CONFIRMED |
| S12 | inn | poisoned member: DOS restores/kills after the night, so the ring-of-regeneration roll (`K:0x400C`) still runs for them; native kills first → fewer draws (`shops.cpp:250-263` before `inn_night_pass`) | (a) RNG, narrow | CONFIRMED |
| S13 | tavern | "had enough to drink" counter `[0xBD20]` is never reset (only 3 refs, all in S2) — fires once per program run; native resets per visit (`begin_shop` `s = {}`) → karma −1 can recur every visit | (a) karma | CONFIRMED (indirect writes not excluded → PROBABLE for "never") |
| S14 | tavern | every price/"sir" line resets the active player (`S2:0x00B2` `[0x587B]=0xFF`) | (a) state | CONFIRMED |
| S15 | tavern | rumor you cannot pay for ends the visit with no farewell (`S2:0x0607` → `0x0760` exit); native continues (`shop_orchestration.cpp:516-526`) | (a) flow + RNG | CONFIRMED |
| S16 | tavern | rations: `getnum(2)` accepts `-N` which buys until broke/full; native treats negatives as "Hrumph." and (non-DOS UI) accepts 10 digits | (a) input domain | CONFIRMED |
| S17 | dispatcher | mounted check `(tile&0xFE)==0x12`; native `(tile&0xFC)==0x10` (`shop_orchestration.cpp:709`) also refuses tiles 0x10/0x11 (a horse just boarded or only moved N/S) | (a) gate | CONFIRMED |
| S18 | horse | DOS also closes the stable when the 32-slot actor table `0x5C5A` is full; native places a terrain tile, never refuses | (a) | CONFIRMED code / reachability UNKNOWN |
| S19 | healer | Heal "no need" test is `HP == maxHP`; native `HP < maxHP` (differs only if HP > maxHP) | (a) edge | CONFIRMED / reachability UNKNOWN |
| P1 | all | DOS console text vs device panel (lists, prompts, echoes) — see §4 | (b) | CONFIRMED |
| P2 | tavern | **a paid rumor shows nothing**: `pay_rumor` puts subject/place in `ShopResult` and no frontend reads them; the form variant `rand(0,3)` is drawn and discarded (`shop_orchestration.cpp:521-522`) | (b) user-visible bug | CONFIRMED |
| P3 | reagents | DOS checks the 99 cap before the pitch (no pitch, no price); native prints the pitch then "can't carry" | (b) | CONFIRMED |
| P4 | Windows Original | `transactional_services=true` (`alpha_runtime.cpp:4047`) adds Y/N questions to tavern rounds/house drinks that DOS charges immediately | (b) flow, decline possible | CONFIRMED |
| P5 | Windows Original | tavern keys are fixed A/D/R/T (`ui_session.cpp:1843-1845`); DOS letters depend on the tavern (M/A/R/C, C/W/–/T, B/R/–/H, F/S/P/A) — DOS `A` is Ale, native `a` is the food round; shipwright `F`/`S` do nothing; guild `D` unreachable ("Shop selection unavailable", `alpha_runtime.cpp:984`); Space accepted as N at "Anything else for thee?" and the shipwright's "anything else" (DOS: Y/N only) | (b) input | CONFIRMED |
| P6 | shipwright | DOS always says "sir" (`S2:0x085A` compares gender with 0x46) | (b) | CONFIRMED |
| P7 | tavern | one living member → `"for the  of ye"` (no word); native `tavern_alive_word(1)="one"` | (b) | CONFIRMED |

No divergence was found in: base price tables, stock lists, reagent grants (fixed, cap 99),
guild lots (+3/+4/+5, cap 99, no pre-check), blacksmith cap/ammo rules (0x1B/0x1D → 99),
sell gold cap 9999, healer flat prices and Minoc/Skara Brae rules, resurrect path,
inn capacity/rest price/room teleport/night loop/wake, tavern round food/plates/house
drink/wine/drunk effect, ration loop, rumor keyword matcher, horse spot scan and price,
farewell table selection (except S7/S11), and RNG cadence of greetings, farewells,
blacksmith chatter, sell offers, ration pitch and rumor form (except S7/S15).

---

## 3. Per shop type

### 3.1 Blacksmith (Weapons/Armour) — S1:0x12B2, buy 0x0B30/0x09AC, sell 0x0F64/0x0E76

**DOS flow (exact console sequence).**
1. expand DS 0x8018 `"Good @, and welcome to #!"\n` → **any key** → expand DS 0x8036
   `\n$ says,\n"` + ASK `[0x3D46+rand(0,1)*2]` (`Hail, friend! Wouldst thou Buy or Sell?`
   / `Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?`) + DS 0x8042 `" `.
2. Keys: Space → DS 0x8056 `No` → farewell(0). `B` / `S` below. Everything else (incl.
   ESC/RETURN) is re-read.
3. **Buy**: DS 0x8046 `Buy\n\n"` + EXCL `[0x3D4A+r]` (`Very good!\n` …) + INTRO
   `[0x3D52+r]` (`We have:` …) → loop: DS 0x7C44 `\n\n`; for each stock item
   (DS 0x3AE2 + town*8, 0xFF ends): letter `a`+i, DS 0x7C48 `...`, long name
   (`[0x17F6+item*2]`, short `[0x1962+…]` when strlen >= 13), `\n` unless at column 0;
   then `\n` + WHICH `[0x3CB6+r]` (`What may I show thee?` …) + DS 0x7C4C `" `. **No
   prices in the list.** Key `a`..last → echo the lower-case letter → buy_one_item;
   ESC/Space → leave loop → farewell(purchased?); other keys re-read.
   buy_one_item: DS 0x7B5E `\n\n"` + record 8..48 (`[0x3C48+item*2]`, `%`=price) +
   DS 0x7B62 `\n\n` + ASKS `[0x3CA6+r]` (`Wouldst thou buy one?` …) + DS 0x7B66 `" `;
   strict Y/N. N → DS 0x7B6A `No\n\n`. Y → DS 0x7B70 `Yes\n`, then: qty==99 →
   DS 0x7B76 `\n"Thou canst not carry any more!"\n` + DS 0x7B9A `says $.\n\n` + any key;
   gold < price → DS 0x7BA4 `\n"` + BROKE `[0x3CAE+r]` + DS 0x7BA8 `"\nyells $.\n` →
   **out, no farewell**; else pay, drain, qty = 99 for 0x1B/0x1D else +1 (cap 99),
   DS 0x7BB4 `\nSold!\n`. Unless thrown out: DS 0x7BBC `"Anything else,\n` + (bought
   this visit ? (gender 0x0C ? DS 0x7BCE `milady?` : DS 0x7BD6 `sir?`) : DS 0x7BDC
   `then?`), then the list again.
4. **Sell**: DS 0x804E `Sell\n\n"`; nothing to sell (items 0..0x2F) → DS 0x7F20
   `Thou hast nothing to sell!"\ngrowls $.\n`. Else SELLPROMPT `[0x3D2E+r]` + DS 0x7EF0
   `" ` and the **"Arms" window** (title DS 0x7EF4 via `K:0x4E50`, window 1, 5 rows
   `qq-ShortName` drawn by ZSTATS 0x05E2, `--` for 0, cursor glyph 0xFD; keys
   up/down (1/3, 2/4), PgUp/PgDn (0xD5/0xD6, 4 rows), Home/End (0xD3/0xD4),
   RETURN/Space = sell selected, ESC = done). sell_one_item: ammo (0x1B/0x1D) → DS 0x7D32
   `\n\n"We don't deal in used ammunition!"\ngrowls $.\n` → done, **no good-bye**;
   DS 0x7D64 `\n\n"`; base 0 → DS 0x7D8C `That, I cannot buy from thee."\nsays $.`;
   else offer record 49..56 (`rand(0,7)`, `&` = name `[0x3CCE]` or long name, `%` =
   price) + DS 0x7D68 `\n\nDeal?" `, strict Y/N: N → DS 0x7D72 `No`; Y → DS 0x7D76
   `Yes\n\n"Done!"\nsays $.`, gold += price (cap 9999), qty −1. If items remain:
   DS 0x7F06 `\n\n"` + SELLMORE `[0x3D3E+r]` + DS 0x7F0A `" `. On exit (not ammo):
   DS 0x7F0E `\n\n"` + SELLBYE `[0x3D36+r]` + DS 0x7F12 `"\n` + (items left ? DS 0x7F16
   `says $.\n` : DS 0x7F20 `Thou hast nothing to sell!"\ngrowls $.\n`). **Sell never
   prints a farewell.**

**Semantics.** Buy price §1.2 with buyer INT; sell `3*(INT*base & 0xFFFF)/100+1`; caps as
above; drain after buy only. No time cost.

**Native.** `begin_shop` greeting `shop_orchestration.cpp:741-745`, ask `:765-767`,
Buy `:801-811` (rand ×2 + `buy_list` rand), `select(Buy)` `:587-605`, `buy_done`
`:373-388`, Sell `:812-823`/`:606-629`, `after_sell`/`sell_exit` `:228-239`,
`buy_equipment`/`sell_equipment` `shops.cpp:99-126`. RNG cadence identical to DOS.

**Divergences.** S1 (INT), S2/S3 (latent). Presentation: list shows prices; prompts,
echoes and "Anything else, sir?" are not emitted by the core (§4).

### 3.2 Magic seller (reagents) — S1:0x075E, list 0x0666, buy 0x0502

**DOS flow.** greeting → Space/N → DS 0x7A44 `No` → farewell(0); Y → DS 0x7A2C
`Yes\n\n"Fine! We sell:\n\n` → loop: for each slot with price != 0 (DS 0x3A32 +
town*8): **upper-case** letter `A`+n (compacted, max `E`), DS 0x7A16 `...`, name
`[0x3C20+slot*2]` (DS 0x79B4 `Sulfur Ash` … `Mandrake`), `\n`; DS 0x7A1A
`\nThy interest?" `. Keys ESC/RETURN/Space → leave → farewell; `A`..`E` (no echo) →
buy; letter beyond the list → re-read. Buy: qty==99 → DS 0x792C `\n\n"Thou canst not
carry any more!"\n\n` + any key (no pitch, no price); else price = haggle(byte base),
grant = DS 0x3A5A table, DS 0x7952 `\n\n"` + record 139..146 (`^` = grant, `%` = price)
+ DS 0x7956 ` Is this thy need?" ` + (`cursor_x>12` → DS 0x796C `\n:`); strict Y/N:
N → DS 0x7970 `No\n\n"What else?\n\n`; Y → DS 0x7982 `Yes\n`; broke → record 147
(`Thou profaneth my shoppe…OUT!` snarls) → out, no farewell; ok → pay, drain, +grant
cap 99, DS 0x7988 `\n"I thank thee!"\nsays $.\n` + DS 0x79A2 `"Anything else?\n\n`.
There is **no quantity prompt** (the `0x7C82` in `shops.md` §2 is `cursor_x`).

**Native.** `select(Reagent)` `shop_orchestration.cpp:630-642`, `reagent_done` `:389-400`,
`buy_reagent` `shops.cpp:127-142`. Semantics match (fixed grant, cap 99, drain).

**Divergences.** S1, S2; P3 (pitch before the cap message).

### 3.3 Guild master (keys, gems, torches) — S1:0x04A2, list 0x03F6, buy 0x02BA

**DOS flow.** greeting → Space/N → DS 0x7928 `No` → farewell(0); Y → DS 0x7916
`Yes\n\n"We sell:\n\n` → loop: DS 0x78D4 `a.........Keys\n`, 0x78E4 `b.........Gems\n`,
0x78F4 `c......Torches\n\n`, 0x7906 `Thy concern?" `. Keys ESC/Space → leave silently →
farewell; `A`-`C` → echo lower-case → buy; `D` → record 164 (`d\n\n"That offer was for a
limited time only.\n\n`, the record carries its own echo) → list again; others re-read.
Buy: price = haggle(`[0x3BEA + town*8 + item*2]`), DS 0x786E `\n\n"` + record 160..162 +
DS 0x7872 `\n\nInterested?" `; strict Y/N: N → DS 0x7882 `No\n\n"What else, then?\n\n`;
Y → DS 0x789A `Yes\n`; broke → record 163 (`Death comes in many ways…`) → out, no
farewell; ok → pay, drain, keys+3/gems+4/torches+5 cap 99 (no pre-check: gold spent at
99), DS 0x78A0 `\n"Sold!"\nsays $.\n\n"What else, \n` + (buyer gender 0x0C ? `m'lady`
: `m'lord`) + DS 0x78D0 `?\n\n` → list.

**Native.** `select(Guild)` `:643-656`, `guild_done` `:401-412`, `buy_guild_item`
`shops.cpp:143-155`. Semantics match.

**Divergences.** S1, S2; P5 (`D` easter egg unreachable from the panel).

### 3.4 Horse seller — S1:0x07BE

**DOS flow.** Before any text: free actor slot in `0x5C5A` (32×8) and a free neighbour
(dx `{0,0,1,-1}`, dy `{1,-1,0,0}` DS 0x3C38/0x3C40) whose tile is 0x44/0x45/0x05 and has
no object (`K:0x368E`). None → DS 0x7A48 `The stables are closed.\n` (no greeting, no
rand). Price = haggle(`[0x3C30+town*2]`) **before** the greeting. greeting → Space/N →
DS 0x7AB2 `No` → farewell(0); Y → DS 0x7A62 `Yes\n\n"` + record 104 (`%`) + DS 0x7A6A
`\n\nDeal?" `; strict Y/N: N → DS 0x7A74 `No` → farewell(0); Y → DS 0x7A78 `Yes!` →
broke: DS 0x7A7E `\n\n"Thou couldst not afford to ` + DS 0x7A9E `feed it!"\nyells $.\n`
→ out, no farewell; ok: pay, drain, actor slot = tile 0x10 at (x,y,floor), redraw →
farewell(1). One horse per visit.

**Native.** `begin_shop` spot check `:726-736`, `horse()` `:284-309`, `buy_horse`
`shops.cpp:156-164`; terrain tile `kPurchasedHorseTile` (`world_terrain.cpp:53`).

**Divergences.** S1, S2, S17 (dispatcher), S18 (actor table full).

### 3.5 Healer — S1:0x14F8, who 0x137C, pay 0x146A

**DOS flow.** greeting → **strict Y/N** (Space is not N): N → DS 0x80A0 `No` → farewell(1)
(type 6 rows are equal in both tables); Y → DS 0x80A4 `Yes\n\n` + DS 0x80AA `"We have
powers to Cure, Heal, or Resurrect."\n` + DS 0x80DA `says $.\n\n"What is the nature of
thy need?" `. Keys C/H/R/Space/RETURN only (ESC re-read). Space/RETURN → DS 0x81C0
`Nothing` → farewell(1). C → DS 0x8106 `Curing`, H → DS 0x8148 `Healing`, R → DS 0x817E
`Resurrect`; member: party of 1 → member 0, else DS 0x805A `\n\n"Who needs my aid?" ` +
roster picker `K:0x2E8E` (none → DS 0x8072 `No one`). Need tests: Cure status=='P';
Heal status!='D' and HP!=maxHP; Resurrect status=='D'; else DS 0x3D5A `\n\n"Thou hast no
need of this art!"\nsays $.`. Offer: DS 0x810E/0x8150/0x8188 `\n\n"` + (Minoc loc 5,
cure/heal: DS 0x8112/0x8154 `Receive now the Light!"`, free, no question) / DS 0x812A
`I can cure thy poisoned body ` / DS 0x816C `I can heal thee ` / DS 0x818C `I can raise
this unfortunate person from ` + DS 0x81B6 `the dead `, then DS 0x807A `for % gold.\n\n
Wilt thou\npay?" ` (strict Y/N, DS 0x8098 `Yes`/0x809C `No`). Broke: if price <= 100 and
location 7 (Skara Brae) → free; else record 173 (charity) → no service. Paid → AV effect
(`S1:0x13B0`), drain, apply (cure 'G'; heal HP=max; resurrect CAST2 0x05E0 mode 0xFF then
HP = new max). After every branch: status redraw + DS 0x81C8 `\n\n"Is there any other way
in which I may\n` + DS 0x81F2 `aid thee?" ` → the same strict Y/N as the greeting (Y
reprints the powers line).
Prices: heal bytes DS 0x3D86 `{35,40,45,50,55,60,65,70}`, cure bytes DS 0x3D8E
`{20,25,30,35,40,15,10,1}`, resurrect words DS 0x3D96 `{200,215,225,237,247,249,262,270}`
— flat, no INT.

**Native.** `healer_member` `:310-333`, HealerDeal `:453-466`, `healer_heal`
`shops.cpp:211-235`. Semantics match (Minoc, Skara Brae, resurrect, drain only when paid).

**Divergences.** S19 (edge). Presentation: the core emits "It is done." (not a DOS
string) and never record 173 (`HEALER_SORRY_INDEX` unused).

### 3.6 Barkeeper (tavern: food, drink, wine, rations, rumors) — S2:0x066C

Subtype `[0x4D4C+town]` = `{0,0,0,2,3,1,0,2,0}`; letters by subtype: food DS 0x4C1E
`MCBF`, drink DS 0x4C24 `AWRS`, rations DS 0x4C2A `R  P` (none in 1/2), chat DS 0x4C30
`CTHA`.

**DOS flow.** `bd18` (served) = 0. greeting → N/Space → DS 0x9F8E `No` → farewell(bd18);
Y → DS 0x9F92 `Yes\n\n"` + menu record 69..72 (by subtype) + `"` + ` ` (+`\n` if
`cursor_x>14`). Menu keys: Space/ESC/RETURN → farewell(bd18) (no echo); letters below;
chat ignored until served; others re-read. After an action that did not end the visit:
bd18=1 unless it returned 2 (wine Space, rations 0), status, DS 0x9F9A `"Anything else\n
for thee?" ` → **Y/N only**: N → DS 0x9FB6 `No` → farewell(bd18); Y → DS 0x9FBA
`Yes\n\n"` + revisit record 73..76 + `"` + ` ` (+`\n`).
- Food round `S2:0x01D2`: echo letter; `\n\n`; cost = per-head DS 0x4C36 × living
  (status != 'D'); `"` + DS 0x9ACE `That will be ` + number + DS 0x9ADC ` gold for the `
  + DS 0x9AA8..0x9ABE `two`..`six` (nothing for 1) + DS 0x9AEC ` of ye,\n` + (member 0
  gender 0x0B ? `sir` : `milady`) + `.` (**this helper sets `[0x587B]=0xFF`**); broke →
  DS 0x9AF6 `"\n\n"CAN'T PAY?\nBeat it!"\nyells ` + keeper + `.\n` → out; else pay,
  drain, food += living (cap 9999), plate 0x95→0x9B north / 0x9A south, DS 0x9B16
  `\nEnjoy!"\n\n`. Charged immediately — no question.
- Drink `S2:0x01F4`: echo letter; if `[0xBD20]==3`: DS 0x9B22 `\n\n"I beg thy\npardon, `
  + sir/milady + DS 0x9B38 `,"\nsays ` + keeper + DS 0x9B42 `.\n"But haven't\nye had
  enough\nto drink?" ` strict Y/N: Y → DS 0x9B6C `Yes\n\n` (no drink, counter stays 3);
  N → DS 0x9B72 `No!`, drunk 25 turns `[0x5957]`, karma −1 floored, serve. Non-wine:
  the round announcement at 1 gold × living, pay, drain, `[0xBD20]++`, Enjoy. Wine:
  DS 0x9B76 `"Our wine list,\n` + sir/milady + DS 0x9B88 `.\n\n` + 6 lines DS 0x9B8C..
  0x9BE6 + DS 0x9BFA `Thy choice?" `; Space → `\n\n` + record 89 (meagre stock, returns
  2); `A`-`F` → echo + DS 0x9C08 `\n\n"Ah, a fine\nchoice, ` + sir/milady + `.` → broke
  DS 0x9C20 (CAN'T PAY, out) / pay, drain, `[0xBD20]++`, DS 0x9C40 `\nEnjoy!"` + `\n\n`.
- Rations `S2:0x0380`: echo `R`/`P`, `\n\n`, price = haggle(DS 0x4C54), `"` + record
  77..83 (`rand(0,6)`) + DS 0x9C4A `\n\nHow many wouldst\nthou like?" ` + `getnum(2)`;
  0 → DS 0x9C6A `\n\n"Hrumph."` (returns 2); per unit: gold −= price, food += 25 (cap
  9999), stop at 9999; shortfall → DS 0x9C76 `\n\n` + (none bought: food<3 → food +=
  `rand(0,1)+1`, record 90 scraps / else DS 0x9C7A `"Thou hast\nneither gold nor\nneed!
  Out!"\n` + DS 0x9CA4 `yells ` + keeper + DS 0x9CAC `.\n`) → out; (some bought) DS
  0x9CB0 `"Thou canst\nafford only ` + N + DS 0x9CCA `!"\n\n` (**no drain**); complete →
  DS 0x9CD0 `\n\n` + drain.
- Rumor `S2:0x0508`: echo letter, DS 0x9EFC `\n\n"` + DS 0x9F00 `Of what wouldst\nthou hear
  my\nlore, ` + sir/milady + DS 0x9F24 `?"\n\nYou respond:\n` + input_string(15) + DS
  0x9F36 `\n\n`; empty → back; 26 keywords (DS 0x4C74, case-insensitive, first occurrence
  must start a word); unknown → DS 0x9F3A `"That, I cannot help thee with.\n\n` and ask
  again; match → record 84 (`%` = DS 0x4D10 price) + DS 0x9F5C `\n\nFair 'nuff?" ` strict
  Y/N: N → DS 0x9F6C `No\n\n`; Y → DS 0x9F72 `Yes\n\n`, broke → DS 0x9F78 `"Sorry, ` +
  sir/milady + record 91 → **visit ends, no farewell**; ok → pay (no drain), record 85..88
  (`rand(0,3)`, `&` subject DS 0x4CA8, `*` place via DS 0x4CDC/0x4CF6) + DS 0x9F82
  `\nsays ` + keeper + DS 0x9F8A `.\n\n`.

**Native.** `round` `:346-366`, `drink` `:367-372`, Drunk `:565-570`/`:936-937`, Wine
`:664-679`, rations `:949-960`/`:984-1014`, rumor `:961-983`/`:516-526`,
`buy_tavern_round` `shops.cpp:358-386`, `buy_rations` `:185-210`, `pay_rumor` `:387-395`.

**Divergences.** S1/S2 (ration INT), S13, S14, S15, S16; P2, P4, P5, P7.

### 3.7 Shipwright — S2:0x0ABC, menu 0x08A8, pay 0x07E2, deliver 0x080E

**DOS flow.** `bd22`(thrown)=`bd24`(bought)=0. greeting → Space/N → DS 0xA00C `No`;
Y → DS 0xA008 `Yes` + menu; on return: farewell(bd24) unless bd22. Menu: record 119
(`\n\n"We sell ocean-\ngoing Frigates…Which would ye\nlike to see?" `); ESC/Space →
back; other non-F/S keys → record 119 again.
- `F`: DS 0x9FE4 `F\n\n`. If `g_ship_flags[0x6605] & 0xC0` (a ship already bought this
  visit): record 125 (`…dock space is limited…87,000 gold.\n\nAcceptable?" `) Y/N: Y →
  DS 0x9FE8 `Yes` + pay check against **10000** → always record 122 (`What? Cheat me…OUT!`)
  + DS 0x9FC2 `yells $.\n`, bd22=1; N → DS 0x9FEC `No` + menu again. Else price =
  haggle(DS 0x4D66), record 117 + record 126 (`\n\nWilt thou\ntake it?" `) Y/N: N → DS
  0x9FF6 `No` → leave menu; Y → DS 0x9FF0 `Yes\n\n`, gold < price → record 122 + yells,
  out; else flags=0x82 and deliver(pay=1, dock=1).
- `S`: DS 0x9FFA `S\n\n`, price = haggle(DS 0x4D6E), record 118 + 126, Y/N: N → DS
  0xA004 `No` → leave menu; Y → DS 0x9FFE `Yes\n\n` + pay check (cheat → out); then
  flags==0 → flags=0x40, deliver(1,1); flags&0x80 → record 120 (`It will be placed
  onboard thy frigate!`), flags++ (skiff count), deliver(pay=1, dock=0); flags&0x40 only
  → record 121 (`More than one skiff…`), deliver(pay=0, dock=0).
- deliver: dock → record 123 (`"Sold!\nThou canst take delivery at the docks…`) and
  `[0x5953]/[0x5954]` = DS 0x4D76/0x4D7A dock X/Y; pay → gold −= price, bd24=1, **drain**;
  status; record 124 (`\n\nWill there be anything else, `) + (gender==0x46 ? `milady` :
  `sir`, i.e. always `sir`) + DS 0x9FD8 `?" ` → Y/N only: Y → DS 0x9FDC `Yes` → menu;
  N → DS 0x9FE0 `No` → leave.
- Delivery: `MAINOUT:0x0D22-0x0D7F` on the next overworld entry turns flags>=0x40 into one
  actor (tile 0x25 frigate if flags>0x7F else 0x29 skiff, hull 0x63, skiffs = flags&0x3F)
  at the stored dock and clears the flags.

**Native.** `select(Ship)` `:657-663`, ShipDeal `:467-487` (pays, places a ship object
immediately via `place_purchased_ship`, `world_terrain.cpp:49-52`, frigate with 2
skiffs, skiff as its own object), ShipAgain `:431-433`, `buy_ship` `shops.cpp:165-177`.

**Divergences.** S1/S2, **S4, S5**; P5 (F/S keys), P6; core result "She awaits thee at the
dock!" is not a DOS string.

### 3.8 Innkeeper — S3:0x08B4, rest 0x0072, leave 0x02AE, pickup 0x04E6

Buyer INT `[bp-4]` and gender `[bp-0xA]` come from the dispatcher `charIdx`.

**DOS flow.** Entry: `[0x588E]=[0x587A]=0` (time spell off), status redraw, greeting →
Y/N/Space(=N): N → DS 0x505F `No` → farewell(si); Y → DS 0x5062 `Yes` + DS 0x5066
`\n\n$ asks,\n"Art thou here\nto Pick up or\n` + DS 0x508E `Leave a\ncompanion, or\nto
Rest for the\nnight?" `. Menu keys R/L/P/Space (Space → farewell(si), no echo; ESC
re-read). After a non-final action: DS 0x50BD `"Is there\nanything more\nI can do for\n
thee?" ` → Y/N/Space(=N).
- Rest: echo `R`; DS 0x4D84 `\n\n`; guests here (16 records, +0x1F == location) >=
  capacity DS 0x4DC4 → DS 0x4D87 `"I am sorry,\n` + milady/sir + DS 0x4DA0 `, but we\n
  have no room\navailable."\n\n` (continue). Else `"` + record 186..190 (`%` =
  signed haggle(rate DS 0x4D7E × party count)) + DS 0x4DCA `\nWilt thou take\nit?" ` +
  Y/N + DS 0x4DE0 `\n\n`. N → farewell(si). Broke → DS 0x4DE3 `"Highwaymen!\nCheap, at
  that!\nOUT!" ` + DS 0x4E07 `screams\n$.\n` → farewell(si). OK → pay, drain, DS 0x4E13
  `"Have a pleasant\nnight, ` + milady/sir + DS 0x4E37 `!"\nsays $.\n\n`, flash, room
  teleport (DS 0x4E7A/0x4E80), G→S for the night, DS 0x4E44 `Zzzzzz....\n\n`, 12×5 min,
  then 9-min steps with ring roll `K:0x400C` until 06:00 (tiles at 20h/05h), DS 0x4E51
  `Morning!\n`, wake NPCs, per member: HP=max, MP by class, P→D (`\n` + name + DS 0x4E5B
  ` has\npassed away.\n`), S→G; x+1 → **visit ends, no farewell**.
- Leave: echo `L`; room check as above; party of 1 → record 191 (`Lord British is
  missing…Not in my inn!`) → **end, no farewell**; DS 0x4E86 `$ asks,\n"Who will\nstay?" `
  + roster picker; none → DS 0x4EA0 `Nobody\n\n` (continue); `\n\n`; Avatar → DS 0x4EAC
  `Thy friend` (+`s` if party > 2) + DS 0x4EB7 ` will not leave thee!\n\n` and ask again;
  dead → record 192 (morgue) → **end, no farewell**; else DS 0x4ECF `"The rate for\nour
  most comfortable room will be ` + DS 0x4F00 `% gold per month, due at check-out.` + DS
  0x4F24 `\nWilt thou take\nit?" ` + Y/N + DS 0x4F3A `\n\n`; Y → no charge, roster
  rotate, months=0, tag=location, party−1, active-player fix, DS 0x4F3D `"I thank thee."\n
  says $.\n\n`, **visit marked as purchase**.
- Pickup (no echo): party 6 → DS 0x4F57 `\n\nOne must first be left behind!\n\n`; no
  guest → DS 0x4F7A `\n\n"No one here is from thy party!"\nsays $.\n\n`; >1 guest → DS
  0x4FA7 `\n\n"Who will\ncheck out?" ` + GUEST REGISTER window (DS 0x4FC0 `    GUEST`,
  0x4FCA `  REGISTER:\n\n`, names at x=4, bar highlight, up/down, RETURN/Space choose,
  ESC → DS 0x4FD8 `No one\n\n`); 1 guest → that one. **No Y/N**: DS 0x4FE1 `\n\n"That
  will be % gold, please."\n\n"` (signed haggle(rate×10) × max(1,months)); broke → record
  193 (`…GUARDS!"` says) + town alarm → **end, no farewell**; ok → pay, drain, rotate in,
  tag 0, P → D, HP 0, DS 0x5005 `Thy friend has died, by the way."\n`; else **HP = max**,
  MP by class, DS 0x5028 `I hope thou hast found thy stay enjoyable,"\n`; + DS 0x5055
  `says $.\n\n`.

**Native.** Menu `:889-922`, InnRestDeal `:492-509`, `inn_rest` `shops.cpp:236-269`,
`inn_night_pass` `:332-346`, Leave `:877-887`/`:488-491`, `inn_leave` `:270-292`,
pickup `:334-345`/`:894-904`, `inn_pickup` `:293-331`.

**Divergences.** S6, S7, S8, S9, S10, S11, S12. Buyer INT: native uses `first()`
(`:207-212`, `:497`, `:337`, `:910-913`) = `characters[0]` (S1 applies; the inn's signed
formula itself matches). Core texts "It shall be done." and the absence of records
191/192/193 and of the rate line are presentation.

---

## 4. What the core emits today vs the DOS console

The core emits `Message` events and `ShopEvent{Entered,State,Result,Exited}`
(`shop_orchestration.cpp:133-149`); `UiSession` appends `ShopResult.message` to the Shop
channel and adds a key legend (removed in DOS mode, `ui_session.cpp:1961-2010`).

| DOS console element | Emitted by the core? |
|---|---|
| greeting record (7 types), blacksmith welcome + ask | yes (no trailing `" `/`\n:` spacing; blacksmith ask lacks leading `\n`) |
| farewell record | yes, but without `\n\n"` / `says $.`; wrong presence after inn rest/alone/morgue/guards (S7) and table after Leave (S11) |
| pitches: blacksmith buy 8..48, sell offers 49..56, reagents 139..146, guild 160..162, horse 104 | yes |
| broke lines: blacksmith BROKE, reagents 147, guild 163, horse, tavern CAN'T PAY, inn Highwaymen | yes (most) |
| result lines `Sold!`, `I thank thee!`, guild `Sold!`, `Yes!`, `Done!`, `Enjoy!`, `Morning!`, pickup lines | yes (some as `ShopResult.message`) |
| invented lines `It is done.`, `She awaits thee at the dock!`, `It shall be done.`, `Anything else?` (rations) | yes — not DOS strings |
| Y/N echoes `Yes`/`No`, item letter echoes | no |
| all lists (blacksmith, reagents, guild, wine) | no (device panel, with prices DOS never shows except wine) |
| prompts: `Thy concern?`, `Thy interest?`, `Is this thy need?`, `Interested?`, `Deal?`, ASKS, WHICH, SELLPROMPT/MORE/BYE, `Anything else, sir?`, `What else…` | no (variants drawn by the core but not exposed) |
| tavern menus 69..76, price announcement, wine list, drunk warning, ration pitch 77..83, afford-only, scraps 90, meagre 89, rumor prompts/84/85..88/91 | no (rumor answer not shown at all — P2) |
| healer powers/nature/curing…/pitches/charity 173/any-other-way | no |
| shipwright 119..126 | no |
| inn menu, no-room, pitches 186..190, rate line, Zzzz, passed away, who will stay/check out, 191/192/193 | partly (`Morning!`, pickup lines) |

Estimate: the core carries roughly **one quarter to one third** of the DOS console text
(greetings, farewells, pitches, broke/result lines). The prompts, lists, echoes and four
whole sub-flows (tavern talk, healer, shipwright, inn) are not emitted, and several
rand-picked variants (BUY ASKS/WHICH/INTRO/EXCL, SELL PROMPT/MORE/BYE, rumor form) are
drawn but their index is not exposed, so a frontend-only presenter **cannot** reproduce
them. The DS strings already exist in `shop_tables.inc`/`shop_greetings.inc` (`msg_*`,
`BLACKSMITH_*`, `*_INDEX`) but are unused by `shop_orchestration.cpp`.

---

## 5. Recommendations

Core (shared, changes the T-Deck too — each needs the pinned-layer protocol: fix the TS
reference `game/src/core/shops/shops.ts` / `game/src/ui/shop-console.ts` at the layer the
corpus pins, `--check` control, regenerate `native/core/build-shops/*`, token-diff, then
native, RED first):
1. **S1 buyer**: add `ShopSession::buyer` resolved at `begin_shop` exactly as TALK 0x0108
   (active player else first G/P member); use it for every INT (`intel()`,
   `shop_offerings`, inn `first()`) and expose it for gender words.
2. **S4** add `drain(true)` after a paid ship purchase (`shop_orchestration.cpp:483`).
3. **S5** model `g_ship_flags` (persisted like the DOS byte, cleared on overworld entry)
   and deliver at overworld entry, or at least the per-visit refusals/onboard skiffs.
4. **S6** clear `TurnState::time_spell`/`spell_turns` when an inn session begins.
5. **S7/S8/S11** inn: mark Leave as a purchase; make rest/alone/morgue/guards end
   silently (no rand); call `dialogue_alarm` on pickup Gold failure.
6. **S9/S10/S12** inn pickup HP=max, no MP on death, keep months; restore after the
   night loop (move the HP/MP/status pass after `inn_night_pass`).
7. **S13/S14/S15** tavern: make the cup counter session-global (TurnState, not saved);
   reset the active player where 0x00AC runs; end the visit on an unpaid rumor.
8. **S17** use `(tile & 0xFE) == 0x12`.
9. Latent S2/S3/S16/S18/S19: decide explicitly (replicate or record as a deliberate
   divergence); S2 is reachable only with INT > 33.

Presentation:
10. **P2** is a user-visible bug on every frontend: surface the rumor text (subject,
    place and the drawn form index) from the core.
11. For Windows Original, add a **core-side DOS transcript** behind a new
    `ShopServices` flag (e.g. `dos_transcript`, default false so the T-Deck stays
    byte-identical): the core already owns every rand that picks a variant, so it should
    emit the exact §3 sequences (strings already in `shop_tables.inc`) as Message events
    and suppress its invented result strings in that mode. The runtime then hides the
    panel in Original mode except for the two DOS windows ("Arms", GUEST REGISTER).
12. Windows Original input: `transactional_services=false` (P4); per-subtype tavern
    letters and F/S at the shipwright, `D` at the guild, Y/N-only at TavernAgain/ShipAgain
    (P5); `getnum(2)` with sign for rations if S16 is replicated.

## 6. Test plan

- Core RED-first unit tests (style of `native/core/tests/shop_flow_test.cpp`), one per
  semantic item: buyer with active player 2 and with Avatar asleep (blacksmith, inn);
  ship purchase under Falsehood (gold − rand(1,64), RNG trace); frigate twice / frigate
  then skiff / skiff twice in one visit; An Tym then inn rest (clock reaches 06:00);
  rest success → no farewell event, no `shop` rand after the night; pickup broke → alarm
  and no farewell; pickup HP restore; tavern 4th drink across two visits; unpaid rumor
  ends visit; mounted tile 0x10 can enter.
- Corpus: regenerate after the TS fixes; mutation check that each new pin goes red.
- Windows Original transcript goldens (scenario scripts) for: blacksmith buy+sell
  (Arms window), reagents with a 99 cap, guild incl. `D`, tavern subtype 0 (round,
  drunk gate, rations partial, rumor) and wine subtype 1, shipwright frigate+skiff,
  healer in Minoc and Skara Brae (charity), inn rest/leave/pickup with register, horse
  closed/open — each compared line-by-line with the §3 sequences.
- T-Deck preservation differential (a4_ui2 probe, baseline vs HEAD) for every
  presentation-only change; semantic changes are expected to move only the scripted
  hashes they target.

## 7. UNKNOWNs

- Whether any horse town ever fills all 32 actor slots (S18) and whether HP > maxHP is
  reachable (S19).
- Whether some indirect pointer write resets `[0xBD20]` (S13): the immediate-operand
  census across all overlays and ULTIMA.EXE found only the three SHOPPES2 references.
- How the DOS game treats `[0xBD20]` on an in-session load (not checked).
- The Talk command's own turn cost around shops (outside SHOPPES; not investigated).
- 1988 vs Exodus-Upgrade differences in DATA.OVL strings/tables (provenance, §0).
- Exact ZSTATS 0x05E2 row layout beyond `qq-Name` and the 0xFD cursor (Arms window).
