// The inputs the asset preparation takes from this CHECKOUT rather than from the player's own game:
//
//   - three tracked data files under game/src/core/data that the stage scripts read at run time
//     (pack:alpha1 reads AdditionalEnemyFlags.json and ShoppeKeeperMap.json; the fixture generator reads
//     AdditionalEnemyFlags.json and longEquipNames.json). They are project data already in the repository
//     (game/src/core/data/ATTRIBUTION.txt: from Ultima5Redux, (c) 2020 Brad Hannah, MIT) -- no original
//     game file is ever read here;
//   - the identity constants the game checks the two main packs against (exact size + payload CRC32, and
//     the SHA-256 it logs), parsed from the C++ headers so they can never drift from what the game enforces.
//
// Plain JavaScript on purpose: prepare.ts reads it through embedded.ts when run from a checkout, and
// build.mjs calls the same function at build time to freeze the result into the single-file bundle, which
// has no checkout to read from.
import { readFileSync } from "node:fs";
import { join } from "node:path";

export const DATA_DIR = "game/src/core/data";
export const DATA_FILES = ["AdditionalEnemyFlags.json", "ShoppeKeeperMap.json", "longEquipNames.json"];

const IDENTITY_HEADERS = [
  { file: "openu5-assets.bin", header: "native/targets/tdeck/main/asset_pack.h", prefix: "kExpectedAssetPack" },
  { file: "openu5-alpha1-resources.bin", header: "native/targets/tdeck/main/alpha_resources.h", prefix: "kExpectedAlphaResource" },
];

function constant(text, name, pattern, header) {
  const match = new RegExp(`\\b${name}${pattern}`).exec(text);
  if (!match) throw new Error(`${header}: cannot find ${name}`);
  return match[1];
}

/** Reads the identity constants for both main packs from the checkout at `repoRoot`. */
export function readIdentity(repoRoot) {
  const identity = {};
  for (const { file, header, prefix } of IDENTITY_HEADERS) {
    const text = readFileSync(join(repoRoot, header), "utf8");
    const size = Number(constant(text, `${prefix}Size`, "\\s*=\\s*(\\d+)\\s*;", header));
    const crc32 = constant(text, `${prefix}Crc32`, "\\s*=\\s*0x([0-9a-fA-F]{1,8})[uU]?\\s*;", header)
      .toLowerCase().padStart(8, "0");
    const sha256 = constant(text, `${prefix}Sha256`, "\\[\\]\\s*=\\s*\"([0-9a-fA-F]{64})\"", header).toLowerCase();
    identity[file] = { size, crc32, sha256, header };
  }
  return identity;
}

/** Everything the bundle must carry: identity constants plus the data files as base64 (exact bytes). */
export function collectEmbedded(repoRoot) {
  const data = {};
  for (const name of DATA_FILES) data[name] = readFileSync(join(repoRoot, DATA_DIR, name)).toString("base64");
  return { identity: readIdentity(repoRoot), data };
}
