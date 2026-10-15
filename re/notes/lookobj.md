# LOOKOBJ.OVL — Look (describir tile) + View-a-gem (Task 3.12)

Overlay #13, 4560 B (0x11D0), load_seg 0x0a29. 16 funciones + 4 bytes de ceros;
catálogo `re/tools/lookobj_catalog.py` (ledger 100%, 17 segmentos). El pozo (0x0042)
y la fuente (0x0162) los derivó Task 3.8. Las strings de los casos especiales viven
en DATA.OVL (`fileoff = DS + 0x10`).

## cmd_look (0x099c) — pipeline

1. gate `ext_935c()==0` → aborta.
2. target = party + `g_cmb_scratch` (dirección ya pedida por el caller).
3. tile = `*ext_a172(x,y)`; special_idx = `ext_93fe(x,y,g_floor)`.
4. **tile 0x29 "a crytal sphere"** (typo original): `rand(1,30)`; si `roster[idx*32+
   0x55b6] > roll` → "Death vision!"; else "Strange vision!" + `gem_view` (SIN gastar
   gema). Único RNG del camino de objetos.
5. else "Thou dost see": special_idx≠0 → describe_special_object; tile ∈
   **{0x89,0x8A,0xA0,0xA4,0xF8}** (las 5 caras de cartel reales) → read_sign; default →
   look_dispatch.
   *Corrección 2026-07-20 (carril sign-console): el decodificador REAL del cartel es
   LOOKOBJ **0x06f8** (portado en `skin/fiel/sign-box.ts bakedSignRows`); el 0x07e4 que
   alguna nota rotulaba "read_sign" es en realidad un especial de `g_location==4` (stats
   de party), no el cartel.*

## look_dispatch (0x0502) — casos especiales (VERIFICADO del asm)

Primero llama a `look_generic(tile)` (texto base de LOOK2.DAT); luego, por tile:

- **0xE0/E1/E2 poste**: canonicaliza persiguiendo el ancla (0xE0→y−1, 0xE1→x+1,
  0xE2→x−1) hasta que el tile ya no es 0xE0-E2; 0xE3="a flagpole".
- **0x59 cielo** → look_sky: día (6≤h<18) "the sun!" **y DAÑA** al personaje activo
  (0x0383–0x03a4: resuelve g_active_char, llama daño con (active_char,1) + redraw
  0x8670); noche = 80 estrellas `rand(9,182)+rand(9,172)` (~160 draws) + bloque zodiacal.
- **0xA1** pozo (3.8), **(tile&0xFC)==0xD8** fuente (3.8).
- **(tile&0xFE)==0xFA reloj**: `h=g_hour%12` (0→12); imprime `H` + ':' + `MM`
  (g_minute, 2 dígitos, pad '0') + " AM.\n" si g_hour≤0x0b else " PM.\n".
- **0xDE Flame**: por `g_location` → 0x1e "Truth", 0x1f "Love", 0x20 "Courage".
- **0xDF entrada colapsada**: por banda **X** (`bp+6`) → 0x3a Shame, 0x48 Destard,
  0x5b Despise, 0x7e Wrong, 0x80 Doom, 0x9c Covetous, 0xef Hythloth, 0xf0 Deceit.
- default → look_generic + "\n" extra si `ext_7c82()`.

Strings (DATA.OVL): 0x7310 " AM.\n", 0x730a " PM.\n", 0x7316/0x731e/0x7324 Flame,
0x732e..0x736e mazmorras (Deceit/Despise/Destard/Wrong/Covetous/Shame/Hythloth/Doom).

## gem_view (0x10fc) — renderer PURO

Doble bucle **32×32** (el CHUNK entero, NO 11×11): tile → `draw_gem_map_tile`
(categoría `byte[tile+0x1d1a]` → jump-table); marcador parpadeante en la posición del
jugador; polling de tecla. **NO toca g_gems, NO RNG.** El consumo de gema está en el
caller (¿CMDS?).

## Divergencias con el clon (cerradas en `game/src/core/game.ts look()`)

1. **Carteles mal clasificados**: el clon usaba `{160,164,224-227,236-249}` — faltaban
   0x89/0x8A y sobraban 0xE0-0xE3 (postes) y 0xEC-0xF9 (flavor estático). Corregido a
   las **5 caras reales** {0x89,0x8A,0xA0,0xA4,0xF8}.
2. Faltaban los casos especiales: **cielo, reloj, Flame, entrada de mazmorra** — añadidos
   exactos (`lookSpecialDescription`, función pura + test `tests/look.test.ts`).
3. **Postes de cartel 0xE0-0xE2 (LOOK-POLISH1, D-117, 2026-10-14)**: la persecución del ancla
   de look_dispatch (0x0510-0x0550) no estaba portada ni en el clon ni en native, así que
   (L)ook a un poste imprimía la frase de relleno de LOOK2.DAT, "parched desert" (reporte:
   Britain (9,21), Look North). Portada en `game.ts look()` y `native/core/src/look.cpp`:
   0xE0 → y−1, 0xE1 → x+1, 0xE2 → x−1, re-leyendo el tile compositado; los 122 postes de
   TOWNE/CASTLE/KEEP (DWELLING ninguno; BRIT/UNDER ninguno) acaban en un cartel 0xF0..0xF9
   (nunca 0xF8). `docs/WINDOWS_LOOK_ENHANCED_POLISH.md` §2.

## Portado / Pendiente

- **Mirar al SOL daña** (look_sky 0x0383–0x03a4): ✅ **PORTADO (F1.9)**. De día,
  `Game.look()` sobre el cielo (0x59) llama `apply_damage(activo,1)` (kernel 0x2A52) +
  emite `party-changed` (redraw 0x8670). 1 HP literal, sin RNG, puede matar. El daño vive
  en `Game.look()` (no en la pura `lookSpecialDescription`). Fallback del activo sin
  selección: el asm, con `g_active_char==0xff`, llama a un sub sin identificar (0x976c) y,
  si retorna 0, copia `g_cmb_scratch_x` (un BYTE de scratch de combate, NO un índice de
  personaje) a `g_active_char` — indecible desde el asm dumpeado; el clon aproxima al
  miembro 0 (Clase C declarada, BP pendiente).
- **Esfera de cristal (0x29)**: ✅ **PORTADA (#144, `re/notes/bola-144-acta.md`)**. El
  campo de roster **0x55b6** = `record+0x0e` = **INTELIGENCIA** (base del roster DS
  0x55a8, visible dentro del selector mismo, CS 0x4988 @0x4a36; casa con
  `saveNative.ts:186/210`). Es un
  contest INT vs `rand(1,30)` con el EMPATE perdiendo (`ja` @0x0a0e): gana → "Strange
  vision!" (DS 0x750a) + `gem_view` 32×32 **sin gastar gema** (el `dec [g_gems]` es del
  case V, 0x3428); pierde → "Death vision!" (DS 0x74fa) + `apply_damage(idx,1)` (kernel
  0x2A52 — puede MATAR). Los dos residuos que esta nota dejaba abiertos quedan cerrados:
  (a) el `idx` de `0xffffa6f8` sale del selector de PJ de comando en **kernel CS 0x4988**
  (base de near-call 0xa290), el MISMO que ya conducía el (S)earch, con el gate del -1
  ANTES del `rand`; (b) **el trigger NO es el comando (V)iew-gem** — es `cmd_look` sobre
  el tile 0x29, y corta antes del "\nThou dost see\n" (`jne` @0x09e8), así que la bola
  nunca imprime prefijo ni frase de LOOK2. La rectificación importa: mientras la mecánica
  estuvo archivada bajo «(V)iew-gem / renderer = UI», el hueco parecía de presentación y
  era una rama entera del despachador de (L)ook sin portar.
- **Cielo nocturno**: el campo de 80 estrellas + zodíaco es cosmético/RNG; el clon da el
  texto día/noche ("the stars." es aproximación Clase D).
- **look_generic vs TileData.json**: el diff byte-a-byte LOOK2.DAT ↔ el tileset del clon
  queda para Task F.
