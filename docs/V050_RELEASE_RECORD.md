# v0.5.0 release record

## Source, tag and promotion policy

- Published build source: `355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9` (verified RC2).
- Annotated tag: `v0.5.0`, pointing to that actual build source.
- Release: [Ultima V Native v0.5.0 — Windows & T-Deck](https://github.com/thekojac304/ultima-v-native/releases/tag/v0.5.0).
- Preparation and user acceptance date: 2026-10-15, America/New_York. Publication details are recorded after remote verification below.

The project has an established accepted-candidate promotion process. This release intentionally retains the accepted RC2 executable and firmware bytes, including embedded version `0.5.0-rc2` and Git `355d4a1e564c`. No version-only rebuild is substituted for the hardware-accepted firmware. The Windows archive changes only `README-Windows.txt`; EXE, SDL DLL, asset-preparation tools, licenses, controls and BUILD-INFO are preserved byte for byte. The stable download filenames identify promotion status; the retained build identity is explicitly disclosed in player docs and release notes.

Final documentation and the main integration commit are newer than the binary source. The tag does not falsely identify those documentation commits as the compilation source. All release runtime sources are verified unchanged from RC2.

## Artifacts

Local release directory: `C:\Dev\U5Native\releases\v0.5.0\355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `UltimaV-Native-v0.5.0-Windows-x64.zip` | 2,410,452 | `13691a7a7e1d3f506ab9a549d48230ff9dd32aeebec87dfb20c482c8f46f3d62` |
| `UltimaV-Native-v0.5.0-TDeck-Launcher.bin` | 1,075,504 | `62f90df106132ab08c916b245c44cbf91cbb93d6e97f02c0ee33ef8ee7018ed9` |

`SHA256SUMS.txt` accompanies both files. The original RC2 packages remain in `releases/v0.5.0-rc2/<source>/ui-polish`, unchanged. Original RC2 ZIP SHA-256: `d7674bd74aba3259663e3a667d8311faad027286bff8eab5177a853ff4c02770`. The BIN hash is unchanged by promotion.

The Windows ZIP has exactly ten flat allowlisted files, including required SDL3.dll and player/license instructions. No game packs, saves, original files or private development evidence are included. The T-Deck download is a validated ESP32-S3 app image, not a bootloader/partition/full-flash bundle.

## Hardware acceptance provenance

The user explicitly reports: they personally tested final v0.5.0 RC2 firmware on the T-Deck and confirm everything is working correctly. Recorded as **PASS, user-reported**, tied to the full source, exact firmware size and SHA-256 above. This satisfies the hardware acceptance gate.

No serial logs or photographs were provided. No itemized checklist outcomes, measured heap, stack high-water figures or exhaustive edge-case coverage are inferred. Earlier SPP1 PASS and manual Windows acceptance remain separate evidence. [Machine-readable acceptance](V050_HARDWARE_ACCEPTANCE.json).

The verified local manifest `builds/ui-polish/evidence/final-release-manifest.json` has been updated with this acceptance; its prior pending version is preserved in `builds/v050-final/evidence/rc2-manifest-before-acceptance.json`.

## Verification

RC2 engineering records establish:

- Native host: 213/213; Windows: 40/40.
- Relevant TypeScript preservation tests: 11/11; release-tooling tests: 7/7.
- Paced and unpaced differential: each 1,065 walks / 8,514 snapshots. Eight expected menu framebuffer changes in two walks; zero unexpected differences. Game state, input, transcript, turns/clock/RNG, saves and audio match the frozen RC1 reference.
- Capability boundary mutations: 8/8 killed; tooling mutations: 2/2 killed, source restored and tests passed.
- Extracted-package missing-data handling (exit 10), bundled asset preparation, portable boot, Original/Enhanced Look, Enhanced shop and Original DOS panels passed.
- Firmware compile/budget passed under ESP-IDF 6.1 and Xtensa GCC 15.2.0. The accepted final image has 235,216 bytes free in the 1,310,720-byte build partition and 38,608 bytes free in its 1,114,112-byte Launcher allocation.

Final promotion checks and public verification are appended below. Full local logs stay outside tracked source under `builds/v050-final/evidence`; private resource packs generated during smoke tests are not release assets.

Toolchains: MSVC 14.51.36231, Windows SDK 10.0.26100.0, static C/C++ runtime; SDL3 3.4.16; Node 24.18.0; host GCC 16.2.0. The accepted RC2 SDL source override was local and not archive-hash-checked at configure time; that provenance qualification is retained.

## Documentation, metadata and limitations

The README, Native roadmap, Windows/T-Deck player guides, controls, save compatibility, build/architecture guides, packaged Windows README and v0.5.0 notes are updated. Canonical target guides link to them; historical records remain available. The separate OpenU5 browser roadmap is preserved.

Repository description: “A native C++ port of Ultima V for Windows and LilyGO T-Deck Plus, featuring DOS-faithful Original mode, optional enhancements, and preservation-focused gameplay.” Topics retain relevant existing tags and add `retro-gaming`, `windows`, `esp32`, `t-deck`. Homepage stays empty; no separate Native website is established. Repository ownership, visibility, license and default branch remain unchanged.

Existing repository screenshots are retained with provenance. New Windows captures remain local because separate redistribution permission for their copyrighted game imagery was not established; no new device photograph is claimed.

Known limitations: pre-1.0 preservation differences; Windows keyboard-only play and preserved aspect ratio; no Ultima IV transfer; DOS dungeon transfer refused and some transient state omitted; full Native → real DOS play/save → Native round trip not claimed; music needs optional compatible patched assets; no benchmarked CPU/RAM minimum or new measured hardware memory figures.

No gameplay changes, old-release replacement, force-push, history rewrite, worktree deletion or broad workspace reorganization are part of promotion. Unrelated untracked primary-checkout hardware notes are preserved.

## Final local promotion gates

- Fresh native host suite: **213/213 PASS**, 161.71 seconds.
- Fresh Windows suite: **40/40 PASS**, 247.61 seconds, including DOS save import/export and Original/Enhanced UI checks.
- Refreshed ZIP: missing-assets exit 10, bundled preparation from the user's files, byte-identical prepared packs, portable boot and all five packaged scenarios PASS.
- Original RC2 manifest verifier PASS after restoring its installed compiler paths to PATH; the initial missing-compiler failure is preserved in the local log.
- Independent promotion audit PASS: exact ten-file archive allowlist, only README changed, AMD64 executable identity, firmware segment checksum/appended SHA, partition budget, matching source and full artifact hashes, required licenses, 67 local documentation links, and no credential-pattern hits or new files over 10 MB across 348 added source files.
- Commercial game input and prepared-pack directories have no tracked files in the build source. No new screenshots, game packs or saves are added by promotion.

## Public publication and closeout

- Published **2026-10-15T22:37:06.0000000-04:00**, America/New_York; GitHub UTC timestamp `2026-10-16T02:37:06Z`.
- Public release ID `414476379`: https://github.com/thekojac304/ultima-v-native/releases/tag/v0.5.0.
- Verified public, **draft=false**, **prerelease=false**, and latest stable release.
- Anonymous downloads of both platform builds and SHA256SUMS.txt match the local originals byte for byte, including full SHA-256 and exact sizes. GitHub's own upload digests match as well.
- Annotated tag object `706c5ef6a0f76d10ce4dafb4a9a007b8db2a9992` resolves to build source `355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9`.
- At publication, local `main` and remote `origin/main` both identified integration commit `b5539080315bc04c9b6117eccf2efa864fd1b7ff`. That commit preserves both RC2 history and the four prior workspace-documentation commits. This subsequent closeout changes documentation only; its exact final main/origin identity is captured after push in the local `builds/v050-final/evidence/public-verification.json` and final delivery report, avoiding a self-referential commit hash in this file.
- Repository description and all requested topics verified through the anonymous API. Existing relevant topics retained. Homepage remains empty, default branch remains main, and repository remains public.
- v0.4.0 remains public (release ID `403309475`); its asset name, size, SHA-256 and URL match the saved pre-publication baseline.
- Fresh anonymous reads verified README, both player guides, release notes and this record are accessible on main.
- All 16 preexisting linked development worktrees retain their original HEAD/status. Four unrelated untracked primary-checkout hardware-note/log files remain byte-identical. The final release worktree has no unrelated changes; the primary checkout retains those four intentional preexisting untracked files.
- Release-tooling regression tests rerun: **7/7 PASS**. Final archive/runtime/license/link/credential-pattern checks passed after integrating main. Original RC1/RC2 packages and historical recovery material remain in place.

Local evidence includes `public-verification.json`, `promotion-audit.json`, `preservation-closeout.json`, `hardware-acceptance.json`, full host/Windows/package logs and downloaded public assets under `builds/v050-final/evidence`. An initial public-verification report write failed because Python lacked optional timezone data; the complete retry used Windows timezone data and passed. No release assets or source payloads changed during that retry.

Publication is complete. No hardware acceptance or publication gate remains pending. The known limitations above remain documented rather than being treated as release blockers.
