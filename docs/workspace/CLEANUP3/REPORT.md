# CLEANUP3 completed consolidation and closeout

15 October 2026 (closeout evidence uses UTC labels dated 16 October).
**The primary repository is now `C:\Dev\U5Native\source`. All 25 mapped moves
are complete: 23 standalone Ultima V roots were consolidated, plus two Ultima V
children of the shared `_Archive` folder. Zero old Ultima V roots remain directly
under `C:\Dev`. Total directory roots fell from 34 to 12**, including the new
U5Native parent. The 11 unrelated root directories retain their names and locations.

The offline final relocation had already succeeded before this closeout. Its
post-relocation host/Windows validation passed, then firmware compilation stopped
on an Xtensa GCC internal segmentation fault. The user completed that existing
build with Ninja at two jobs. Closeout independently verified the image and
preservation baselines, completed the remaining incremental size checks, and
updated current documentation. **No migration runner was rerun and no directory
was moved, deleted or archived during closeout.** No gameplay, Original/Enhanced,
firmware functionality or Windows functionality changes were made. Hardware
acceptance remains **NOT RUN**.

## Original and final C:\Dev structure

Original, 34 directory roots:

```text
C:\Dev\
  Circadia\
  ClassicUO\
  CrossInk\
  Inform6\
  OpenU5-Native\
  TamaPoke\
  _Archive\
  mobileuo\
  openu5-release-artifacts\
  spp1-hw-package\
  spp2-scratch\
  system-shock-android\
  system-shock-baseline\
  ultima-v-archive\
  ultima-v-builds\
  ultima-v-native-parity\
  ultima-v-native-rc1\
  ultima-v-native-spp2\
  ultima-v-native-windows\
  uo-offline\
  uo-offline-dev\
  uvw-base\
  uvw-dos-base\
  uvw-dos-shops\
  uvw-dos2-base\
  uvw-look-polish\
  uvw-look-polish-evidence\
  uvw-lp-base\
  uvw-original-dos\
  uvw-original-dos-evidence\
  uvw-present\
  uvw-shops-base\
  uvw-spp1-base\
  uvw-spp2-base\
```

Final, 12 directory roots:

```text
C:\Dev\
  Circadia\
  ClassicUO\
  CrossInk\
  Inform6\
  TamaPoke\
  U5Native\
  _Archive\
  mobileuo\
  system-shock-android\
  system-shock-baseline\
  uo-offline\
  uo-offline-dev\
```

The 12 remaining directories comprise the single U5Native parent and 11 unrelated
projects/containers. No Ultima V-specific root is temporarily retained outside it.
The shared `_Archive` parent retains its five unrelated children. The new layout is:

```text
C:\Dev\U5Native\
  source\                         main checkout and common native .git
  worktrees\                      all 15 linked native checkouts
  builds\                         fresh validation outputs; legacy-layout instructions
  releases\historical\            retained SPP1 and DOS/LOOK packages/evidence
  releases\legacy-layout\         preserved CLEANUP2 release convention
  archive\openu5\                 independent repository, including wip3 stash
  archive\u5-ti89\                complete historical TI project
  archive\investigations\         complete spp2-scratch investigation
  archive\legacy-layout\          preserved CLEANUP2 archive convention
  archive\CLEANUP3\               all recovery copies and migration evidence
  tools\                          current workspace helper and historical finisher
```

## Every executed root move

Sources are relative to `C:\Dev`; destinations are relative to `C:\Dev\U5Native`.
[actual-moves.csv](actual-moves.csv) now contains all 25 successful journal receipts;
[mapping.csv](mapping.csv) is the original complete plan.
[detailed-mapping.csv](detailed-mapping.csv) retains original per-root file counts,
Git baseline HEAD/branch, modified/untracked/ignored counts, purpose and dependencies,
and records every actual destination and the relocated common Git store. Its main
HEAD is the preservation baseline preceding the authorized closeout commit.

| Former source | Actual destination | Status/purpose |
| --- | --- | --- |
| `OpenU5-Native` | `source` | Active development; primary — main |
| `_Archive\openu5` | `archive\openu5` | Historical/reference; repository — enhanced-mode |
| `ultima-v-native-parity` | `worktrees\ultima-v-native-parity` | Active development; worktree — parity/shared-preservation-1 |
| `ultima-v-native-rc1` | `worktrees\ultima-v-native-rc1` | Verified RC1 reference; physical acceptance deferred; worktree — release/v0.5.0-rc1 |
| `ultima-v-native-spp2` | `worktrees\ultima-v-native-spp2` | Active development; worktree — parity/shared-preservation-2 |
| `ultima-v-native-windows` | `worktrees\ultima-v-native-windows` | Active development; worktree — windows/initial-port |
| `uvw-base` | `worktrees\uvw-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `uvw-dos-base` | `worktrees\uvw-dos-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `uvw-dos-shops` | `worktrees\uvw-dos-shops` | Active development; worktree — windows/original-dos-shops |
| `uvw-dos2-base` | `worktrees\uvw-dos2-base` | Historical baseline with active uncommitted DOS work; worktree — DETACHED |
| `uvw-look-polish` | `worktrees\uvw-look-polish` | Active development; worktree — windows/look-enhanced-polish |
| `uvw-lp-base` | `worktrees\uvw-lp-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `uvw-original-dos` | `worktrees\uvw-original-dos` | Active development; worktree — windows/original-dos-interaction |
| `uvw-present` | `worktrees\uvw-present` | Active development; worktree — windows/presentation-wip |
| `uvw-shops-base` | `worktrees\uvw-shops-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `uvw-spp1-base` | `worktrees\uvw-spp1-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `uvw-spp2-base` | `worktrees\uvw-spp2-base` | Historical comparison checkout; all local files retained; worktree — DETACHED |
| `spp1-hw-package` | `releases\historical\spp1-hw-package` | Historical/reference; hardware rollback package |
| `uvw-look-polish-evidence` | `releases\historical\uvw-look-polish-evidence` | Historical/reference; acceptance evidence |
| `uvw-original-dos-evidence` | `releases\historical\uvw-original-dos-evidence` | Historical/reference; acceptance evidence |
| `spp2-scratch` | `archive\investigations\spp2-scratch` | Historical/reference; unique investigation |
| `_Archive\u5-ti89` | `archive\u5-ti89` | Historical/reference; historical TI project/tools |
| `ultima-v-archive` | `archive\legacy-layout` | Superseded CLEANUP2 convention; instructions preserved; CLEANUP2 archive convention |
| `ultima-v-builds` | `builds\legacy-layout` | Superseded CLEANUP2 convention; instructions preserved; CLEANUP2 builds convention |
| `openu5-release-artifacts` | `releases\legacy-layout` | Superseded CLEANUP2 convention; instructions preserved; CLEANUP2 releases convention |

The staged migration moved low-dependency artifacts/archives first, then build
convention roots and linked checkouts. Linked checkouts used `git worktree move`.
The final primary move used a same-volume rename followed by coordinated
`git worktree repair`. Its successful `moves/00.json` receipt points to a fresh
primary recovery copy created before relocation. The interrupted source-lock
record and earlier 24-move report remain historical evidence, not current blockers.

## Independent preservation and Git validation

The original CLEANUP1/CLEANUP2 inventory, classifications and approval records were
reused. Closeout did not regenerate or rewrite their frozen evidence. The final
full live audit verified **219,227 original physical files, 55,557 directories,
54 junctions, 774 selected preservation hashes, all 24 frozen inventory checksums
and all 41 actual release-catalog entries**. Earlier prose describing 39 catalog
entries was inaccurate; the immutable catalog itself has 41 and was not edited.
Original game inputs, packages and historical evidence retain their bytes.

All 17 scoped Git states matched their pre-migration baseline before the closeout
commit: primary + 15 linked checkouts, and the independent archived repository.
HEADs, branches, refs, stashes, existing reflog entries, staged content, uncommitted
binary diffs, untracked files and ignored inventories were compared. The only
permitted primary edits were cleanup documentation and the cleanup tool's backup
path. All 16 native registrations resolve to `source\.git`, with no prunable or
locked registrations. Every existing junction resolves directly to its translated
real target; no old-path compatibility junction remains.

The RC1 checkout is clean on `release/v0.5.0-rc1` at
`a96656b2330097c18c52b78a8840d3749c0e9854`.
The DOS-base checkout retains its exact two-line CMake edit and three untracked
probe inputs. The archived repository remains at
`2485950ed6e0038794fc7a62b35cde01bcbffb46`; its `wip3` stash is still
`81211c6226aaa94a72145d040f51f345f4479dbb`.

Native `git fsck --connectivity-only --no-dangling` retains the pre-existing exit 2
warning about invalid `refs/remotes/origin/HEAD` (stale cached remote default).
No reachable object is missing. Archived Git connectivity returns 0. No refs were
repaired, fetched, reset or deleted to conceal that warning.

## Recovery and generated-file handling

All original recovery copies remain at `archive\CLEANUP3\recovery`: 219,227 files,
29,723,603,917 logical bytes. The offline finisher's additional primary recovery
is `archive\CLEANUP3\finish-20261016T002344Z\primary-recovery`: 58,542 files,
6,345,833,056 bytes. Closeout independently rehashed all 26 recovery roots,
checked exact inventories and distinct physical file identities against live data.
Together they contain **277,769 files and 36,069,436,973 bytes**. Junctions are
recorded in manifests and intentionally omitted as live links in copies.
The prior restore exercise for archived Git/wip3 and DOS work, and forward/inverse
Git rehearsal, remain intact. These independent copies share C: and provide no
protection against drive loss.

Passing drift tests rewrote 24 derived fixture outputs with identical bytes but new
timestamps in the primary and RC1. Closeout checked every recorded before/after
hash, retained separate output copies, and restored only their original mtimes.
No contents were rewritten. The subsequent full live inventory audit passed.
Historical CMake/ESP-IDF caches, command logs and investigation scripts remain
byte-for-byte snapshots with former absolute paths. They are retired outputs,
not unresolved dependencies of current build entry points. They were not edited
or reused for the relocated validation builds.

Current path consumers checked include workspace.json, the canonical build helper,
workspace READMEs, the source workspace guide, Git configuration and cleanup tool.
All point to the new layout. The four fresh build caches point to their actual
relocated primary/RC1 sources. Shared TamaPoke w64devkit, Espressif and Visual Studio
dependencies remain external and unchanged. Saved editor/Codex entries, if still
naming the former path, must be reselected at `C:\Dev\U5Native\source`.

## Post-relocation builds and tests

The interrupted finisher's immutable JSON receipts, stdout/stderr logs and CTest
`LastTest.log` files were independently cross-checked, including executable/source
paths and result counts. These tests were not needlessly rebuilt or repeated.

| Check | Confirmed result |
| --- | --- |
| Primary developer-enabled host build/tests | 202/202 pass |
| RC1 host build/tests | 212/212 pass |
| RC1 Windows build/tests | 38/38 pass |
| RC1 release tooling | 5 pass |
| Cleanup safety | 15 pass, 1 skipped for Windows symlink privilege |
| Original post-relocation firmware `ninja -j 6 all` | Exit 1; Xtensa GCC ICE, preserved |
| User's resumed firmware `ninja -j 2` | User-reported exit 0; resulting image independently verified |
| Independent incremental firmware `ninja -j 2 all` | Exit 0; size/budget checks only, no source recompilation/linking |
| Hardware/physical release acceptance | NOT RUN; deferred |

The initial migration also retained a developer-disabled primary-host attempt
that failed to link `apply_debug_teleport`. Its successful fresh developer-enabled
attempt matched retained local configurations; no CMake/gameplay fix was made.
The offline post-relocation developer-enabled validation above passed.

### Firmware failure and retry are separate records

The failed original attempt is retained unchanged in
`archive\CLEANUP3\finish-20261016T002344Z\tests\firmware-build.json` and `.log`.
It failed while compiling ESP-IDF's `esp_lcd_panel_rgb.c` with Xtensa GCC 15.2.0,
not project source diagnostics. Its log SHA-256 is
`0107f9ae1c00f8ff28eb7d63d4fa51f35034c9db66f624b3322b6e2191cad344`.

The user's retry reused exactly
`builds\cleanup3-finish-20261016T002344Z\rc1-tdeck` with two Ninja jobs.
No contemporaneous manual-retry console log was supplied. Its exit 0 is explicitly
attributed to the user; it is corroborated by the valid image, ELF identity and
successful Ninja log entries for the formerly failing object and linked ELF.
The separate closeout incremental run has its own captured log and observed exit 0.
Dry runs for both application and bootloader first confirmed no compilation or
linking was pending; image and ELF hashes were identical before and after the run.
No firmware source, build flags or packaging behavior was changed.

Completed image: `builds\cleanup3-finish-20261016T002344Z\rc1-tdeck\openu5_tdeck.bin`.

| Firmware property | Independently verified value |
| --- | --- |
| Image size | 1,075,088 bytes |
| App partition / free | 1,310,720 / 235,632 bytes |
| Image SHA-256 | `b73831bf957d70d22296b30001bb74865e03b8f67b43594c05a56e4112bc5c79` |
| ELF SHA-256 embedded in image | `7670d27dfc3967176eb634c35f96dbcec649abe1399fc493647af38a11d65036` |
| Embedded project/version/SDK | `openu5_tdeck`, `0.5.0-rc1`, ESP-IDF `v6.1` |
| Embedded source marker | clean `a96656b23300`, matching the clean RC1 checkout |
| SDK configuration | Build/source sdkconfig hashes match; target ESP32-S3, flash 16 MB |
| Image integrity | Header, segments, XOR checksum, appended SHA-256 valid |
| Build source | Actual relocated RC1 target in project_description and CMake cache |

Preserved RC1 Windows ZIP: 2,411,152 bytes, SHA-256
`97e2ae5201076d2f96e51e80063382f7116e39c52d3cbb337faf285e2db51e54`.
Preserved RC1 Launcher image: 1,075,088 bytes, SHA-256
`742dffcfc2675d02473e1566951af7ff36d743b205051aaac40d72f6893ec787`.
The completed fresh firmware has a different hash. Reproducible-build mode is off,
so source/configuration and embedded ELF provenance are verified without claiming
binary reproducibility. Both historical packages and new outputs are retained;
no package was regenerated, overwritten or substituted.

## Closeout evidence, commit and remaining limitations

The additive closeout directory is
`C:\Dev\U5Native\archive\CLEANUP3\closeout-20261016T011631Z`:

- `interrupted-receipts.json`: hashes and checks for the 11 offline command receipts
  (10 successes and the original firmware failure), plus CTest result evidence.
- `firmware-verification.json`: image/ELF/configuration/compiler/partition provenance.
- `firmware-retry.json` and `firmware-incremental-check.log`: user retry attribution
  and separately observed successful incremental validation; original failure hashes.
- `recovery-verification.json`: independent hashes/inventories for all 26 copies.
- `paths-verification.json`: current 54-link graph, 16 registrations and active consumers.
- `documents-before-closeout/`: copies of the superseded blocked-state documents.
- `closeout-staging.json`: exact approved paths/blob identities before committing.
- `git-after-commit.json`: commit-aware final verification of all 17 Git states,
  preserved prior reflog entries and all refs except the single authorized main advance.

The full live audit receipt is separately retained at
`archive\CLEANUP3\final-relocated-closeout-20261016T011631Z.json`.
Its strict pre-commit verifier deliberately expects the original main HEAD/index;
the documentation commit legitimately changes that baseline. The separate
commit-aware check verifies exactly one child commit, its approved file set/blob
identities, an empty index, and preserved unrelated work. See
[README.md](README.md) for read-only verification commands.

The closeout commit includes this report, mapping/validation records, current
workspace guide and reviewable workspace/verification tools. The only existing
project-tool edit changes cleanup_builds.py's documented/default backup path to
`C:\Dev\U5Native\releases\backups`; deletion/approval gates remain unchanged.
It excludes the three pre-existing `native/core/a4-final-hw-*.log` files and
`native/targets/tdeck/A4-FINAL-HW.md`, which remain untracked. Resolve the commit
with `git log -1 --format=%H -- docs/workspace/CLEANUP3/REPORT.md`; the final response
and external post-commit receipt record its hash. No push or publication occurred.

**No directory migration remains pending.** Remaining limitations are deferred
hardware testing, the existing remote-HEAD metadata warning, one privilege-based
symlink test skip, absent contemporaneous manual-retry console capture, and recovery
copies sharing the live volume. The compiler ICE was bypassed by the successful
two-job retry; the compiler itself was not repaired. Keep the earlier developer-off
host-link failure visible as a configuration limitation.

## Recovery / rollback

Preserve all recovery copies, frozen inventories, failed attempts and new receipts.
[ROLLBACK.md](ROLLBACK.md) records the rehearsed inverse sequence. Before any
separately authorized rollback, stop writers and save the closeout commit and all
subsequent work, refuse existing old destinations, inspect all 25 move receipts,
reverse the coordinated primary/linked moves with Git repair/worktree operations,
and recreate links from manifests. Restore into a separate location if live data
is unavailable. Do not reset, clean, prune, overwrite copies or rerun either
one-time migration script as part of closeout. No rollback was performed here.
