/**
 * SPP2 (ledger D-111) — lo que imprime un (T)alk que no abre conversación, contra el binario.
 *
 * El brazo T del kernel (0x33c2) responde al aire libre «Funny, no response!» (DS 0xa216). En un
 * pueblo el Talk es de TALK 0x041c: sin NPC en la celda (0x048a, tras el paso del mostrador
 * 0x046b) imprime DS 0x91d6 «\nNobody's here!\n» (0x0491) y devuelve 0. Un NPC con
 * dialogNumber 0 lo contesta `talk_converse_dispatch` 0x031e: «No response!» (0x0364,
 * DS 0x9168), o para un guardia (tipo de objeto 0x70, 0x0371) «The guard offers\nno response!»
 * (0x0387, DS 0x9148), salvo que la IA de su ranura servida sea 4 (0x0350 salta el test).
 *
 * El port nativo hace lo mismo (native/core/src/dialogue_orchestration.cpp; spp2_runtime T1-T5).
 */
import { describe, expect, it } from "vitest";
import { Game, type GameData } from "../src/core/game.js";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import type { SmallMapLocation, WorldData } from "../src/core/world/map.js";
import { NpcManager, type NpcRuntime } from "../src/core/npc/manager.js";
import { TalkScriptRegistry } from "../src/core/dialogue/registry.js";

const TOWN = 2;
const PARTY = { x: 10, y: 10 };

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Avatar", gender: 0x0b, class: "A", status: "G", strength: 20, dexterity: 20,
    intelligence: 20, currentMp: 10, currentHp: 50, maxHp: 60, exp: 0, level: 2,
    monthsAtInn: 0, helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff,
    amulet: 0xff, partyStatus: 0, ...over,
  };
}

function makeState(location: number): GameState {
  return {
    characters: [makeChar(), makeChar({ name: "Iolo" })],
    partySize: 2,
    activeCharacter: 0,
    food: 100, gold: 100, keys: 5, gems: 3, torches: 2, karma: 40,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 35 },
    turnsSinceStart: 0,
    position: { location, floor: 0, x: PARTY.x, y: PARTY.y },
    transport: "foot",
    prevHour: 12,
    npcDead: Array.from({ length: 32 }, () => []),
    npcMet: Array.from({ length: 32 }, () => []),
  } as unknown as GameState;
}

function makeWorld(): WorldData {
  const tiles = Array.from({ length: 32 }, () => Array.from({ length: 32 }, () => 5));
  const loc: SmallMapLocation = { id: TOWN, name: "Britain", floors: [{ z: 0, tiles }] };
  const overworld = Array.from({ length: 256 }, () => Array.from({ length: 256 }, () => 5));
  return { overworld, underworld: overworld, smallMaps: new Map([[TOWN, loc]]) };
}

const gameData: GameData = {
  locationsX: Array.from({ length: 32 }, () => 100),
  locationsY: Array.from({ length: 32 }, () => 100),
  locationNames: Array.from({ length: 32 }, (_, i) => `Loc${i + 1}`),
};

/** Un NPC al ESTE de la party (o ninguno). */
function partida(location: number, npc: Partial<NpcRuntime> | null): Game {
  const vivo = npc
    ? ({ slot: 3, location: TOWN, type: 0x40, dialogNumber: 0, aiTypes: [1, 1, 1], times: [0, 0, 0, 0],
         servedSlot: 0, x: PARTY.x + 1, y: PARTY.y, z: 0, ...npc } as unknown as NpcRuntime)
    : null;
  const manager = {
    setRng() {}, enterMap() {}, tick() {},
    npcsAt: (loc: number, floor: number) => (vivo && loc === TOWN && floor === 0 ? [vivo] : []),
    npcAt: (loc: number, floor: number, x: number, y: number) =>
      vivo && loc === TOWN && floor === 0 && x === vivo.x && y === vivo.y ? vivo : null,
  } as unknown as NpcManager;
  return new Game({} as ExtractedInitialState, makeWorld(), gameData, makeState(location), {
    npcManager: manager,
    talkScripts: new TalkScriptRegistry({ towne: [], dwelling: [], castle: [], keep: [] }),
  });
}

describe("SPP2 D-111 — el Talk sin conversación imprime la cadena del binario", () => {
  it("en un pueblo, sin NPC en la celda: «Nobody's here!» (TALK 0x0491, DS 0x91d6)", () => {
    const g = partida(TOWN, null);
    expect(g.talkTarget("east")).toBeNull();
    expect(g.talkRefusal("east")).toBe("Nobody's here!");
  });

  it("control: al aire libre, el kernel 0x33c2 «Funny, no response!» (DS 0xa216)", () => {
    expect(partida(0, null).talkRefusal("east")).toBe("Funny, no response!");
  });

  it("un NPC con dialogNumber 0 que no es guardia: «No response!» (TALK 0x0364, DS 0x9168)", () => {
    const g = partida(TOWN, { type: 0x50, dialogNumber: 0 });
    expect(g.talkTarget("east")).toBeNull();
    expect(g.talkRefusal("east")).toBe("No response!");
  });

  it("un guardia (tipo 0x70) con dialogNumber 0: «The guard offers\\nno response!» (TALK 0x0387, DS 0x9148)", () => {
    expect(partida(TOWN, { type: 0x70, dialogNumber: 0 }).talkRefusal("east")).toBe("The guard offers\nno response!");
  });

  it("…salvo que la IA de su ranura servida sea 4 (0x0350 salta el test del guardia): «No response!»", () => {
    expect(partida(TOWN, { type: 0x70, dialogNumber: 0, aiTypes: [1, 4, 1], servedSlot: 1 }).talkRefusal("east")).toBe(
      "No response!",
    );
  });
});
