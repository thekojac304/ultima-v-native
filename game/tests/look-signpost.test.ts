/**
 * (L)ook a un POSTE de cartel (LOOK-POLISH1, D-117), con los assets REALES.
 *
 * look_dispatch (LOOKOBJ 0x0510-0x0550) persigue el poste/brazo de un cartel colgante
 * (0xE0 → y−1, 0xE1 → x+1, 0xE2 → x−1) re-leyendo el tile hasta que deja de ser uno de
 * los tres, y describe ESE tile: el cartel. La frase LOOK2.DAT de los postes ("parched
 * desert") es un relleno que el original nunca imprime. Reporte (Windows Original):
 * Britain, la calzada de ladrillo, Look North → "Thou dost see parched desert".
 */
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { beforeAll, describe, expect, it } from "vitest";
import { Game, type GameData } from "../src/core/game.js";
import { NpcManager, type NpcSlot } from "../src/core/npc/manager.js";
import type { WorldData, SmallMapLocation } from "../src/core/world/map.js";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import type { Direction } from "../src/core/world/movement.js";

const ASSETS = resolve(dirname(fileURLToPath(import.meta.url)), "../assets");
const readJson = <T>(p: string): T => JSON.parse(readFileSync(`${ASSETS}/${p}`, "utf-8")) as T;

const BRITAIN = 2;

let world: WorldData;
let smallMaps: SmallMapLocation[];
let init: ExtractedInitialState;
let look2: string[];
let npcData: Record<number, NpcSlot[]>;

beforeAll(() => {
  smallMaps = readJson<SmallMapLocation[]>("maps/smallmaps.json");
  world = {
    overworld: readJson("maps/overworld.json"),
    underworld: readJson("maps/underworld.json"),
    smallMaps: new Map(smallMaps.map((l) => [l.id, l])),
  };
  init = readJson("initial-state.json");
  look2 = readJson<string[]>("look2.json");
  npcData = readJson<Record<number, NpcSlot[]>>("npcs.json");
});

const gameData = (): GameData => ({
  locationsX: Array.from({ length: 33 }, () => 250),
  locationsY: Array.from({ length: 33 }, () => 250),
  locationNames: Array.from({ length: 33 }, (_, i) => `Loc${i}`),
  look2,
});

/** Un Look desde (x,y) en location/floor; sin NPCs salvo que se pida el manager. */
function lookFrom(location: number, floor: number, x: number, y: number, dir: Direction, npcs = false): string[] {
  const state: GameState = createNewGame(init);
  state.position = { location, floor, x, y };
  state.time.hour = 12;
  const game = new Game(init, world, gameData(), state, npcs ? { npcManager: new NpcManager(npcData) } : undefined);
  return game.look(dir).filter((e) => e.kind === "message").map((e) => (e as { text: string }).text);
}

describe("Look at a sign's post (LOOKOBJ 0x0510)", () => {
  it("LOOK2.DAT's phrase for 0xE0..0xE2 is the placeholder the game never prints", () => {
    expect([look2[0xe0], look2[0xe1], look2[0xe2]]).toEqual(["parched desert", "parched desert", "parched desert"]);
    expect(look2[0xf2]).toBe("the sign of an armoury");
  });

  it("the report: Britain (9,21), Look North -> the armoury's sign, not parched desert", () => {
    expect(lookFrom(BRITAIN, 0, 9, 21, "north")).toEqual(["Thou dost see the sign of an armoury"]);
    expect(lookFrom(BRITAIN, 0, 8, 21, "north")).toEqual(["Thou dost see the sign of an armoury"]);
    expect(lookFrom(BRITAIN, 0, 10, 21, "north")).toEqual(["Thou dost see the sign of an armoury"]);
  });

  it("all four directions follow the chain", () => {
    expect(lookFrom(BRITAIN, 0, 8, 18, "south")).toEqual(["Thou dost see the sign of an armoury"]);
    expect(lookFrom(BRITAIN, 0, 11, 8, "east")).toEqual(["Thou dost see the sign of a provisioner"]);
    expect(lookFrom(BRITAIN, 0, 19, 8, "west")).toEqual(["Thou dost see the sign of an inn"]);
    expect(lookFrom(BRITAIN, 0, 12, 10, "north")).toEqual(["Thou dost see the sign of a provisioner"]);
  });

  it("the sign itself and ordinary terrain are unchanged", () => {
    expect(lookFrom(BRITAIN, 0, 9, 20, "north")).toEqual(["Thou dost see the sign of an armoury"]);
    expect(lookFrom(BRITAIN, 0, 15, 21, "north")).toEqual(["Thou dost see cobble"]);
    expect(lookFrom(BRITAIN, 0, 12, 21, "north")).toEqual(["Thou dost see grass"]);
  });

  it("every post of every 1988 small map names its chain's sign (122 posts)", () => {
    let posts = 0;
    for (const loc of smallMaps)
      for (const floor of loc.floors)
        for (let y = 0; y < 32; y++)
          for (let x = 0; x < 32; x++) {
            let t = floor.tiles[y]![x]!;
            if (t < 0xe0 || t > 0xe2) continue;
            posts++;
            let cx = x;
            let cy = y;
            while (t >= 0xe0 && t <= 0xe2) {
              if (t === 0xe0) cy--;
              else if (t === 0xe1) cx++;
              else cx--;
              t = floor.tiles[cy]![cx]!;
            }
            expect(t >= 0xf0 && t <= 0xf9 && t !== 0xf8, `${loc.name} z${floor.z} (${x},${y}) ends on a sign`).toBe(true);
            const below = y < 31;
            expect(
              lookFrom(loc.id, floor.z, x, below ? y + 1 : y - 1, below ? "north" : "south"),
              `${loc.name} z${floor.z} (${x},${y})`,
            ).toEqual([`Thou dost see ${look2[t]}`]);
          }
    expect(posts).toBe(122);
  });

  it("Britain with its NPCs: the report's Look is the sign too", () => {
    expect(lookFrom(BRITAIN, 0, 9, 21, "north", true)).toEqual(["Thou dost see the sign of an armoury"]);
  });
});
