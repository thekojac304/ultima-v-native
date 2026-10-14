/**
 * Ultima V Native -- make the game's resource packs from the player's OWN copy of Ultima V, in one command.
 *
 *   node prepare-assets.mjs --game <Ultima V folder> --out <folder> [options]         (the bundle, build.mjs)
 *   node --import tsx native/tools/asset-prep/prepare.ts --game <dir> --out <dir> ...  (from a checkout)
 *
 * It runs the project's existing pack pipeline, unchanged, in this order:
 *   1. extract      extractor/src/cli.ts                               original DOS files -> game/assets
 *   2. fixtures     native/core/tools/generate-world-flow-fixtures.ts  -> native/core/fixtures (fixed-enemies.txt)
 *   3. pack-native  native/tools/u5pack/cli.ts                         -> openu5-assets.bin
 *   4. alpha1       native/tools/u5pack/alpha1.ts                      -> openu5-alpha1-resources.bin
 *   5. audio        native/tools/u5pack/audio.ts (its functions)       -> openu5-audio.bin (optional)
 * Every intermediate lands in a work directory (a fresh temporary one unless --work is given) through the
 * stage scripts' explicit path options, so nothing is ever written into a checkout or into the game folder.
 * Stages 1-4 run as child processes of this same script (`--stage <name> <stage options>`): each keeps its
 * own argv, exit code and memory exactly as when npm runs it. The finished packs are checked against the
 * identity constants the game enforces (exact size + payload CRC32; the SHA-256 it logs) and only then copied
 * into --out. The last line printed is always `ASSET_PREP_RESULT {json}`, also on failure.
 *
 * Exit codes: 0 done; 1 a stage or the copy failed; 2 bad command line or game folder; 3 --out already holds
 * different pack files (pass --force to replace them); 4 the packs do not match what this version of the game
 * expects (nothing installed unless --allow-mismatch).
 */
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import {
  closeSync, existsSync, mkdirSync, mkdtempSync, openSync, readFileSync, readdirSync, renameSync, rmSync,
  statSync, unlinkSync, writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { REQUIRED_FILES, inspectSourceFiles } from "../../../extractor/src/pipeline.js";
import { buildAudioPack, describe as describeAudio } from "../u5pack/audio.js";
import { detectMusicCapability, directorySource } from "../u5pack/audio-capability.js";
import { crc32, inspectAssetPack } from "../u5pack/format.js";
import { EMBEDDED, type PackIdentity } from "./embedded.js";

// ---------------------------------------------------------------------------------------------------------
// Stage mode: `<this script> --stage <name> ...` imports one stage script, which reads its own options from
// process.argv at module level, exactly as under `npm run`. String-literal imports, so the bundle carries
// them (esbuild initialises a dynamically imported module only when it is imported).
const STAGES: Record<string, () => Promise<unknown>> = {
  extract: () => import("../../../extractor/src/cli.js"),
  fixtures: () => import("../../core/tools/generate-world-flow-fixtures.js"),
  "pack-native": () => import("../u5pack/cli.js"),
  alpha1: () => import("../u5pack/alpha1.js"),
};

const ASSETS_PACK = "openu5-assets.bin";
const ALPHA_PACK = "openu5-alpha1-resources.bin";
const AUDIO_PACK = "openu5-audio.bin";
const WORK_MARKER = ".ultima-v-asset-prep";

// Original files the chain needs beyond the extractor's own REQUIRED_FILES (which inspectSourceFiles checks,
// with exact sizes where known). pack:alpha1 reads the first group directly; the extractor treats the second
// as optional, but the resource pack embeds what it makes from them, so without any one of them the pack can
// never match the identity the game checks.
const EXTRA_REQUIRED = [
  "DNG1.16", "DNG2.16", "DNG3.16", "ITEMS.16",
  ...Array.from({ length: 8 }, (_, i) => `MON${i}.16`),
  "END1.16", "END2.16", "TEXT.16", "ENDSC.16", "PROPORT.PCS",
  "INIT.OOL", "CREATE.16", ...Array.from({ length: 6 }, (_, i) => `STORY${i + 1}.16`),
  "ULTIMA.16", "STARTSC.16", "TITLE.BIT", "BRITISH.BIT",
];

class Failure extends Error {
  constructor(public exitCode: number, message: string, public usage = false) { super(message); }
}

interface Options {
  game: string; out: string; work?: string;
  noAudio: boolean; force: boolean; keepWork: boolean; allowMismatch: boolean; verbose: boolean;
}

interface FileReport {
  name: string; size: number; crc32: string; sha256: string;
  expected: PackIdentity | null;
  /** Size + payload CRC32 equal to the game's constants (the gate the game applies); null = no lock. */
  match: boolean | null;
  sha256Match: boolean | null;
  action: "written" | "unchanged" | "not-installed" | "pending";
  path: string | null;
}

const USAGE = `Ultima V Native - make the game's resource packs from your own copy of Ultima V.

Usage:
  node prepare-assets.mjs --game <Ultima V folder> --out <output folder> [options]

  --game DIR         the folder with the original DOS game files (TILES.16, DATA.OVL, BRIT.DAT, ...)
  --out DIR          where the packs go: Ultima V Native's data folder, normally
                     "%APPDATA%\\Ultima V Native\\ultima5", or "<its folder>\\data\\ultima5" when a
                     portable.txt sits beside UltimaVNative.exe
  --work DIR         keep every intermediate file in DIR (default: a temporary folder, deleted afterwards)
  --no-audio         do not make openu5-audio.bin (music needs the music-patched game anyway)
  --force            replace pack files in --out that differ from the new ones
  --keep-work        keep the temporary work folder even when everything succeeds
  --allow-mismatch   install packs even if they differ from what this version of the game expects
  --verbose          show every stage's own output
  --help             this text

Nothing is downloaded, and the original game files are only read. The last line printed is a
machine-readable summary: ASSET_PREP_RESULT {json}. Exit codes: 0 done, 1 a step failed,
2 bad options or game folder, 3 different packs already in --out (see --force), 4 packs that do
not match this version of the game (see --allow-mismatch).`;

function parseOptions(argv: string[]): Options | "help" {
  const values: Record<string, string> = {};
  const flags = new Set<string>();
  const valued = new Set(["--game", "--out", "--work"]);
  const boolean = new Set(["--no-audio", "--force", "--keep-work", "--allow-mismatch", "--verbose"]);
  for (let i = 0; i < argv.length; i++) {
    let arg = argv[i]!;
    let inline: string | undefined;
    const eq = arg.indexOf("=");
    if (arg.startsWith("--") && eq > 0) { inline = arg.slice(eq + 1); arg = arg.slice(0, eq); }
    if (arg === "--help" || arg === "-h" || arg === "/?") return "help";
    if (valued.has(arg)) {
      const value = inline ?? argv[++i];
      if (value === undefined || value === "" || (inline === undefined && value.startsWith("--"))) {
        throw new Failure(2, `${arg} needs a folder.`, true);
      }
      values[arg] = value;
    } else if (boolean.has(arg) && inline === undefined) {
      flags.add(arg);
    } else {
      throw new Failure(2, `Unknown option: ${argv[i]}`, true);
    }
  }
  if (!values["--game"]) throw new Failure(2, "Missing --game <Ultima V folder>.", true);
  if (!values["--out"]) throw new Failure(2, "Missing --out <output folder>.", true);
  // A trailing quote is what cmd leaves when a folder ending in a backslash is quoted ("C:\Games\U5\").
  const clean = (p: string) => resolve(p.replace(/"+$/, ""));
  return {
    game: clean(values["--game"]),
    out: clean(values["--out"]),
    work: values["--work"] ? clean(values["--work"]) : undefined,
    noAudio: flags.has("--no-audio"), force: flags.has("--force"), keepWork: flags.has("--keep-work"),
    allowMismatch: flags.has("--allow-mismatch"), verbose: flags.has("--verbose"),
  };
}

const key = (p: string) => (process.platform === "win32" ? resolve(p).toLowerCase() : resolve(p));
const inside = (child: string, parent: string) => {
  const c = key(child), p = key(parent);
  return c === p || c.startsWith(p.endsWith(sep) ? p : p + sep);
};
const sha256 = (b: Uint8Array) => createHash("sha256").update(b).digest("hex");
const hex8 = (n: number) => (n >>> 0).toString(16).padStart(8, "0");
const seconds = (ms: number) => `${(ms / 1000).toFixed(1)} s`;
const bytes = (n: number) => n.toLocaleString("en-US");

/** Looks one and two levels below `dir` for a folder that holds the game, to suggest it. */
function findGameBelow(dir: string): string | undefined {
  const looksLikeGame = (d: string) => {
    try {
      const names = new Set(readdirSync(d).map((n) => n.toUpperCase()));
      return names.has("DATA.OVL") && names.has("TILES.16") && names.has("BRIT.DAT");
    } catch { return false; }
  };
  const children = (d: string) => {
    try { return readdirSync(d, { withFileTypes: true }).filter((e) => e.isDirectory()).map((e) => join(d, e.name)); }
    catch { return []; }
  };
  for (const a of children(dir)) {
    if (looksLikeGame(a)) return a;
    for (const b of children(a)) if (looksLikeGame(b)) return b;
  }
  return undefined;
}

function checkGameFolder(game: string): number {
  if (!existsSync(game) || !statSync(game).isDirectory()) {
    throw new Failure(2, `The game folder does not exist: ${game}`);
  }
  const inspection = inspectSourceFiles({
    exists: (name) => existsSync(join(game, name)),
    read: (name) => new Uint8Array(readFileSync(join(game, name))),
  });
  const required = new Set([...Object.keys(REQUIRED_FILES), ...EXTRA_REQUIRED]);
  const missing = [...inspection.missing];
  for (const name of EXTRA_REQUIRED) {
    if (!(name in REQUIRED_FILES) && !missing.includes(name) && !existsSync(join(game, name))) missing.push(name);
  }
  if (missing.length === 0 && inspection.sizeMismatch.length === 0) return required.size;
  const lines = [`The folder does not hold a complete copy of the DOS Ultima V:`, `  ${game}`, ""];
  if (missing.length > 0) {
    lines.push(`Missing original file(s) (${missing.length}):`);
    for (let i = 0; i < missing.length; i += 8) lines.push(`  ${missing.slice(i, i + 8).join("  ")}`);
  }
  if (inspection.sizeMismatch.length > 0) {
    lines.push("Present, but not the size of the DOS release this port was verified against " +
               "(another edition, or a damaged copy):");
    for (const m of inspection.sizeMismatch) lines.push(`  ${m.name}: ${m.size} bytes, expected ${m.expected}`);
  }
  const below = missing.length > 0 ? findGameBelow(game) : undefined;
  lines.push("");
  if (below) lines.push(`The game files seem to be in a folder below it. Try:  --game "${below}"`, "");
  lines.push("Point --game at the folder that contains TILES.16, DATA.OVL and BRIT.DAT (for example the",
             "folder of the GOG or Steam install that holds ULTIMA.EXE).");
  throw new Failure(2, lines.join("\n"));
}

function prepareWork(opts: Options): { work: string; temporary: boolean } {
  if (opts.work) {
    const work = opts.work;
    if (inside(work, opts.game) || inside(opts.game, work)) {
      throw new Failure(2, `The work folder must be outside the game folder: ${work}`);
    }
    if (existsSync(work)) {
      if (!statSync(work).isDirectory()) throw new Failure(2, `--work is not a folder: ${work}`);
      const entries = readdirSync(work);
      if (entries.length > 0 && !entries.includes(WORK_MARKER)) {
        throw new Failure(2, `The work folder is not empty and was not made by this tool: ${work}\n` +
                             "Give an empty or new folder.");
      }
      for (const sub of ["game", "native", "logs"]) rmSync(join(work, sub), { recursive: true, force: true });
    }
    mkdirSync(work, { recursive: true });
    writeFileSync(join(work, WORK_MARKER), "Work folder of native/tools/asset-prep (Ultima V Native).\n");
    return { work, temporary: false };
  }
  return { work: mkdtempSync(join(tmpdir(), "ultima-v-asset-prep-")), temporary: true };
}

interface StageReport { name: string; label: string; ms: number; ok: boolean; log: string | null }

function runStage(self: string, name: string, label: string, args: string[], work: string, verbose: boolean,
                  index: number, total: number): StageReport {
  const prefix = `[${index}/${total}] ${label}`;
  const log = join(work, "logs", `${name}.log`);
  mkdirSync(dirname(log), { recursive: true });
  if (verbose) console.log(`${prefix} ...`);
  else process.stdout.write(`${prefix.padEnd(46, " ")} `);
  const fd = verbose ? null : openSync(log, "w");
  const started = performance.now();
  const result = spawnSync(process.execPath, [...process.execArgv, self, "--stage", name, ...args], {
    stdio: verbose ? "inherit" : ["ignore", fd!, fd!],
    windowsHide: true,
  });
  if (fd !== null) closeSync(fd);
  const ms = Math.round(performance.now() - started);
  const ok = result.status === 0 && !result.error;
  if (verbose) console.log(`${prefix}: ${ok ? "done" : "FAILED"} (${seconds(ms)})`);
  else console.log(`${ok ? "done" : "FAILED"}  ${seconds(ms)}`);
  if (!ok) {
    if (!verbose && existsSync(log)) {
      const tail = readFileSync(log, "utf8").trimEnd().split(/\r?\n/).slice(-25);
      console.log(`\n--- last lines of ${log} ---\n${tail.join("\n")}\n---`);
    }
    const why = result.error ? result.error.message : `exit code ${result.status ?? result.signal}`;
    throw new Failure(1, `Stage "${name}" failed (${why}).` + (verbose ? "" : ` Full output: ${log}`));
  }
  return { name, label, ms, ok, log: verbose ? null : log };
}

/** Size, payload CRC32 (the value the game's identity gate reads) and SHA-256 of one produced pack. */
function describePack(name: string, data: Buffer): FileReport {
  let payloadCrc: number;
  if (name === ASSETS_PACK) {
    payloadCrc = inspectAssetPack(new Uint8Array(data)).payloadCrc32; // validates the whole layout too
  } else {
    // OU5A1RES / OU5AUDIO: u32 count at 16, file size at 20, payload CRC32 at 24, payload after the TOC.
    if (data.length < 32 || data.readUInt32LE(20) !== data.length) throw new Failure(1, `${name}: bad header`);
    const toc = name === ALPHA_PACK ? 64 : 48;
    const payloadStart = 32 + data.readUInt32LE(16) * toc;
    payloadCrc = data.readUInt32LE(24);
    if (crc32(data.subarray(payloadStart)) !== payloadCrc) throw new Failure(1, `${name}: payload CRC32 does not verify`);
  }
  const expected = EMBEDDED.identity[name] ?? null;
  const report: FileReport = {
    name, size: data.length, crc32: hex8(payloadCrc), sha256: sha256(data), expected,
    match: null, sha256Match: null, action: "pending", path: null,
  };
  if (expected) {
    report.match = report.size === expected.size && report.crc32 === expected.crc32;
    report.sha256Match = report.sha256 === expected.sha256;
  }
  return report;
}

function printPack(r: FileReport): void {
  const check = (ok: boolean, expected: string) => (ok ? "ok" : `DIFFERENT, expected ${expected}`);
  const size = `${bytes(r.size)} bytes`;
  console.log(`  ${r.name}`);
  if (r.expected) {
    const e = r.expected;
    console.log(`    size     ${size.padEnd(20)} ${check(r.size === e.size, `${bytes(e.size)} bytes`)}`);
    console.log(`    CRC32    ${r.crc32.padEnd(20)} ${check(r.crc32 === e.crc32, e.crc32)}  (payload; the game's identity gate)`);
    console.log(`    SHA-256  ${r.sha256} ${check(r.sha256Match === true, e.sha256)}`);
    console.log(`    => ${r.match ? "MATCHES" : "DOES NOT MATCH"} what this version of the game expects (${e.header})`);
  } else {
    console.log(`    size     ${size}`);
    console.log(`    CRC32    ${r.crc32}  (payload)`);
    console.log(`    SHA-256  ${r.sha256}`);
    console.log("    => optional pack, no identity lock");
  }
}

function install(reports: FileReport[], files: Map<string, Buffer>, out: string, force: boolean): void {
  mkdirSync(out, { recursive: true });
  const conflicts: string[] = [];
  for (const r of reports) {
    const dest = join(out, r.name);
    r.path = dest;
    if (existsSync(dest)) {
      if (sha256(readFileSync(dest)) === r.sha256) r.action = "unchanged";
      else conflicts.push(r.name);
    }
  }
  if (conflicts.length > 0 && !force) {
    for (const r of reports) if (r.action === "pending") r.action = "not-installed";
    throw new Failure(3, `The output folder already holds different ${conflicts.join(", ")}:\n  ${out}\n` +
                         "Nothing was replaced. Run again with --force to replace them with the new packs.");
  }
  for (const r of reports) {
    if (r.action === "unchanged") continue;
    const dest = r.path!;
    const temp = `${dest}.tmp-${process.pid}`;
    try {
      writeFileSync(temp, files.get(r.name)!);
      renameSync(temp, dest);
    } catch (error) {
      try { if (existsSync(temp)) unlinkSync(temp); } catch { /* keep the original error */ }
      const code = (error as NodeJS.ErrnoException).code;
      const hint = code === "EPERM" || code === "EBUSY" || code === "EACCES"
        ? " Close Ultima V Native if it is running, then try again." : "";
      throw new Failure(1, `Cannot write ${dest}: ${(error as Error).message}.${hint}`);
    }
    if (sha256(readFileSync(dest)) !== r.sha256) throw new Failure(1, `${dest} did not read back identically.`);
    r.action = "written";
  }
}

async function orchestrate(argv: string[]): Promise<number> {
  const started = performance.now();
  const self = resolve(process.argv[1]!);
  const summary: Record<string, unknown> = {
    ok: false, exitCode: 1, tool: { origin: EMBEDDED.origin, commit: EMBEDDED.commit || null, node: process.version },
  };
  let work: string | null = null;
  let temporaryWork = false;
  let keepWork = false;
  let exitCode = 1;
  // In the bundle the stage scripts see a module URL inside a folder that must never appear (build.mjs);
  // if it does, some stage fell back to a checkout default instead of the explicit paths given to it.
  const noCheckoutRoot = EMBEDDED.origin === "bundle"
    ? resolve(dirname(fileURLToPath(import.meta.url)), "../../..") : null;
  const noCheckoutExisted = noCheckoutRoot !== null && existsSync(noCheckoutRoot);
  try {
    const major = Number(process.versions.node.split(".")[0]);
    if (major < 18) throw new Failure(2, `Node.js ${process.version} is too old; install Node.js 18 or newer (LTS).`);
    const parsed = parseOptions(argv);
    if (parsed === "help") {
      console.log(USAGE);
      exitCode = 0;
      summary.ok = true;
      summary.help = true;
      return 0;
    }
    const opts = parsed;
    keepWork = opts.keepWork;
    summary.game = opts.game; summary.out = opts.out;
    if (key(opts.out) === key(opts.game)) {
      throw new Failure(2, "--out must not be the game folder; the original files are never written to.");
    }

    console.log("Ultima V Native - resource pack preparation" +
                (EMBEDDED.commit ? ` (tool built from ${EMBEDDED.commit})` : " (checkout)"));
    console.log(`  game folder    ${opts.game}`);
    console.log(`  output folder  ${opts.out}`);
    process.stdout.write("\nChecking the game folder ... ");
    const present = checkGameFolder(opts.game);
    console.log(`${present} required original files present.`);

    const prepared = prepareWork(opts);
    work = prepared.work; temporaryWork = prepared.temporary;
    summary.work = work;
    console.log(`  work folder    ${work}${temporaryWork ? " (temporary)" : ""}\n`);

    const assets = join(work, "game", "assets");
    const data = join(work, "game", "src", "core", "data");
    const core = join(work, "native", "core");
    const packs = join(work, "native", "assets");
    mkdirSync(data, { recursive: true });
    mkdirSync(packs, { recursive: true });
    for (const [name, base64] of Object.entries(EMBEDDED.data)) writeFileSync(join(data, name), Buffer.from(base64, "base64"));

    const total = opts.noAudio ? 4 : 5;
    const stages: StageReport[] = [];
    summary.stages = stages;
    const stage = (name: string, label: string, args: string[]) =>
      stages.push(runStage(self, name, label, args, work!, opts.verbose, stages.length + 1, total));
    stage("extract", "Extracting the original data", ["--src", opts.game, "--out", assets]);
    stage("fixtures", "Deriving the combat tables", ["--assets", assets, "--data", data, "--source", opts.game, "--out", core,
      "--skip-loot-names"]);
    stage("pack-native", `Building ${ASSETS_PACK}`, ["--source", opts.game, "--output", join(packs, ASSETS_PACK)]);
    stage("alpha1", `Building ${ALPHA_PACK}`, ["--assets", assets, "--data", data, "--fixtures", join(core, "fixtures"),
      "--source", opts.game, "--output", join(packs, ALPHA_PACK)]);
    if (!opts.noAudio) {
      process.stdout.write(`[5/5] Building ${AUDIO_PACK}`.padEnd(46, " ") + " ");
      const t = performance.now();
      const detection = detectMusicCapability(directorySource(opts.game));
      const pack = buildAudioPack(detection);
      writeFileSync(join(packs, AUDIO_PACK), pack);
      writeFileSync(join(work, "logs", "audio.log"), describeAudio(detection).join("\n") + "\n");
      const songs = [...Array(16).keys()].filter((i) => (detection.songsValid >> i) & 1).length;
      summary.audio = { capability: detection.capability, songs, reason: detection.reason };
      stages.push({ name: "audio", label: `Building ${AUDIO_PACK}`, ms: Math.round(performance.now() - t), ok: true,
                    log: join(work, "logs", "audio.log") });
      console.log(`done  ${seconds(performance.now() - t)}  (music: ${detection.capability}` +
                  `${songs ? `, ${songs} songs` : ""})`);
    }
    if (noCheckoutRoot && !noCheckoutExisted && existsSync(noCheckoutRoot)) {
      throw new Failure(1, `A stage wrote outside the work folder (${noCheckoutRoot}); please report this.`);
    }

    const names = [ASSETS_PACK, ALPHA_PACK, ...(opts.noAudio ? [] : [AUDIO_PACK])];
    const files = new Map(names.map((n) => [n, readFileSync(join(packs, n))] as const));
    const reports = names.map((n) => describePack(n, files.get(n)!));
    summary.files = reports;
    console.log("\nPacks:");
    for (const r of reports) printPack(r);
    console.log("");

    const mismatched = reports.filter((r) => r.match === false).map((r) => r.name);
    if (mismatched.length > 0 && !opts.allowMismatch) {
      for (const r of reports) r.action = "not-installed";
      keepWork = true;
      const plural = mismatched.length > 1;
      throw new Failure(4,
        `${mismatched.join(" and ")} ${plural ? "do" : "does"} not match what this version of the game expects, so the\n` +
        `game would refuse ${plural ? "them" : "it"}. Usually this means a different edition of Ultima V (this port was verified\n` +
        "against the DOS release, e.g. the GOG copy) or modified game files. Nothing was installed;\n" +
        `the packs are kept in ${packs}. (--allow-mismatch installs them anyway.)`);
    }

    install(reports, files, opts.out, opts.force);
    for (const r of reports) console.log(`  ${r.action === "written" ? "wrote    " : "unchanged"}  ${r.path}`);
    exitCode = 0;
    summary.ok = true;
    console.log(`\nDone in ${seconds(performance.now() - started)}. Ultima V Native will find the packs in:\n  ${opts.out}`);
  } catch (error) {
    exitCode = error instanceof Failure ? error.exitCode : 1;
    const message = error instanceof Failure ? error.message : (error as Error)?.stack ?? String(error);
    summary.error = message;
    console.log(`\nERROR: ${message}`);
    if (error instanceof Failure && error.usage) console.log("(--help lists the options)");
    if (work && temporaryWork && exitCode === 1) keepWork = true; // keep the evidence of a failed stage
  } finally {
    if (work && temporaryWork && !keepWork) {
      rmSync(work, { recursive: true, force: true });
      summary.work = null;
      for (const s of (summary.stages as StageReport[] | undefined) ?? []) s.log = null;
    } else if (work) {
      summary.workKept = true;
      if (exitCode !== 0) console.log(`The work folder was kept: ${work}`);
    }
    summary.exitCode = exitCode;
    summary.elapsedMs = Math.round(performance.now() - started);
    console.log(`ASSET_PREP_RESULT ${JSON.stringify(summary)}`);
  }
  return exitCode;
}

async function stageMain(name: string | undefined): Promise<number> {
  const load = name ? STAGES[name] : undefined;
  if (!load) {
    console.error(`Unknown stage "${name ?? ""}". Stages: ${Object.keys(STAGES).join(", ")}.`);
    return 2;
  }
  await load();
  return 0;
}

// No top-level await: under tsx this file is compiled as CommonJS (the repository root package.json has no
// "type"), like the u5pack scripts.
const args = process.argv.slice(2);
(args[0] === "--stage" ? stageMain(args[1]) : orchestrate(args)).then(
  (code) => { process.exitCode = code; },
  (error) => { console.error(error); process.exitCode = 1; },
);
