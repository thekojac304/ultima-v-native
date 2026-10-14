// Checkout flavour of the embedded inputs (see embedded-sources.mjs): read from this repository when
// prepare.ts runs through tsx. build.mjs REPLACES this module in the single-file bundle with the same
// object frozen at build time (origin "bundle"), because the bundle runs without any checkout.
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
// @ts-ignore -- plain JavaScript module shared with build.mjs
import { collectEmbedded } from "./embedded-sources.mjs";

export interface PackIdentity { size: number; crc32: string; sha256: string; header: string }
export interface EmbeddedInputs {
  origin: "checkout" | "bundle";
  /** Git commit the bundle was built from ("" in a checkout run). */
  commit: string;
  identity: Record<string, PackIdentity>;
  /** game/src/core/data file name -> exact bytes, base64. */
  data: Record<string, string>;
}

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
export const EMBEDDED: EmbeddedInputs = { origin: "checkout", commit: "", ...collectEmbedded(repoRoot) };
