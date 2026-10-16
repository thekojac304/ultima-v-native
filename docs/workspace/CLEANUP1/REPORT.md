# Ultima V Native CLEANUP1 workspace audit

Audit date: 15 October 2026. **Preservation audit complete; no repositories,
worktrees, packages, builds, evidence, branches or original files moved or deleted.**
Gameplay, Original/Enhanced behavior, firmware functionality and Windows behavior
are unchanged. RC1 physical acceptance remains **NOT RUN**.

The audit covered **22 relevant roots, 219,131 files, 55,550 directories and
29,505,915,949 logical bytes (27.48 GiB)**. It recorded **54 resolving reparse
points**, **11,671 file hashes**, and **2,229 hash-identical groups**. Collection
reported no file-read/traversal errors. Unrelated C:\Dev projects were identified
at their roots and left untouched.

## Deliverables and how to read them

The permanent [workspace guide](../../WORKSPACE_ORGANIZATION.md) gives the proposed
directory tree and ongoing rules. This report is the reviewable cleanup plan.
The complete local census is in `inventory/`:

| File | Contents |
| --- | --- |
| [files.csv](inventory/files.csv) | Every physical file: absolute path, logical size, timestamp, attributes, Git state, selected SHA-256 |
| [directories.csv](inventory/directories.csv) | Every physical directory and recursive logical size/file count |
| [git-and-workspace.json](inventory/git-and-workspace.json) | Full HEADs, remotes, branches/upstreams, status, staged/unstaged stats, tracked/untracked/ignored lists, tags, stashes, worktree metadata, root inventory and collection timestamps |
| [classified-files.csv](inventory/classified-files.csv) | One classification, exact path/size, Git relationship, proposed action and justification for every file |
| [classified-directories.csv](inventory/classified-directories.csv) | Every directory's conservative container policy; child file classifications are authoritative |
| [reparse-points.csv](inventory/reparse-points.csv) | All links, targets and accessibility; link targets are not counted a second time |
| [release-catalog.csv](inventory/release-catalog.csv) | 39 Windows ZIPs, Launcher images and app images; SHA-256, version/source evidence and retention decision |
| [duplicate-review.csv](inventory/duplicate-review.csv) | Every hashed duplicate member, designated retained copy and dependency-aware retention decision |
| [approval-plan.csv](inventory/approval-plan.csv) | Exact candidate paths, destinations, sizes, actions, justifications and approval prerequisites |
| [generated-object-provenance.csv](inventory/generated-object-provenance.csv) | Generated objects mapped to existing source/compiler and retained exact command databases; retained in place |
| [path-dependencies-all.txt](inventory/path-dependencies-all.txt) | 2,105 script/tool/CMake matches across all native worktrees and loose investigation roots |
| [path-dependencies.txt](inventory/path-dependencies.txt) | Earlier, narrower generated path-dependency listing; moved into the local inventory during CLEANUP2 without changing its bytes |
| [preservation-verification.json](inventory/preservation-verification.json) | 63 passing preservation/receipt checks, post-audit Git states and the separate Git metadata warning |
| [fresh-rc1-tooling-tests.log](inventory/fresh-rc1-tooling-tests.log) | Fresh 5/5 release-tooling regression run |
| [preexisting-dos2-unstaged.patch](inventory/preexisting-dos2-unstaged.patch) | Read-only capture of the existing two-line DOS-base CMake edit |
| [archived-openu5-stash-stat.txt](inventory/archived-openu5-stash-stat.txt) | Existing `wip3` stash identity and scope |
| [audit-files.sha256](inventory/audit-files.sha256) | Checksums of the completed local census and derived reports |

These local census outputs are narrowly ignored to avoid committing large,
machine-specific path lists. They currently occupy about **205 MiB**; this audit
reclaimed **0 bytes**. They remain on disk and are not a Git-backed archive. Preserve
them separately before any future cleanup. The collectors and this summary remain
ordinary repository files. No backup or restoration exercise is claimed.

`files.csv` records physical files without traversing junctions. Git's own ignored
lists can include paths through those junctions, so their counts are intentionally
different. Blank hashes mean the file was inventoried but not hashed. Hashing covers
packages, runtime binaries, original inputs, untracked local files and selected
ignored evidence; it does not prove every source file or compiler intermediate
byte-identical to another copy. Sizes are logical bytes, not allocated NTFS space;
hardlink allocation, compression and cluster overhead were not measured. Nested
directory totals overlap and must not be summed.

## Git repositories and working trees

Git metadata establishes **one native repository with a primary checkout and 15
linked worktrees**. Seven linked checkouts have detached HEADs. All registered
paths exist; none is marked locked/prunable in the recorded porcelain. No `.git`
lock files were found in the scoped census. Lock-free enumeration does not prove
that Windows will permit a later move of an open executable or folder.

The native common object store is `C:\Dev\OpenU5-Native\.git`. Remotes:
`origin=https://github.com/thekojac304/ultima-v-native.git` and
`enhanced=https://github.com/thekojac304/OpenU5-Enhanced.git`.

`C:\Dev\_Archive\openu5` is a **separate repository**, not a native worktree. Its
HEAD is `2485950ed6e0038794fc7a62b35cde01bcbffb46` on `enhanced-mode`, with
`origin=https://github.com/thekojac304/OpenU5-Enhanced.git` and
`upstream=https://github.com/kokoima/openu5.git`. Its clean checkout contains
**stash `wip3`, object `81211c6226aaa94a72145d040f51f345f4479dbb`**, affecting 103
files including substantial mobile/UI/audio work. Preserve its Git store, stash,
private files and evidence as a unit. Shared ancestry or equal HEADs do not make
these repositories disposable duplicates.

`C:\Dev\_Archive\u5-ti89` is non-Git historical material: TI binaries,
documentation, reverse-engineering databases, execution/validation evidence,
Ghidra/JDK/Python tools and port-readiness analyses. Keep it in its existing archive.

The unrelated root inventory identifies Circadia, ClassicUO, CrossInk, Inform6,
mobileuo, system-shock-android, system-shock-baseline, TamaPoke, uo-offline,
uo-offline-dev and the other archive children. Root Git identities were recorded
where present, without recursively auditing their contents or changing them.
**TamaPoke's w64devkit is an existing native build dependency**, so its unrelated
status does not make that toolchain relocatable.

| Exact root under C:\Dev | MiB | HEAD / branch | Initial work state | Classification and action |
| --- | ---: | --- | --- | --- |
| `OpenU5-Native` | 5,844.19 | `96a55ff9084f / main` | 4 untracked; 0 tracked edits | KEEP — ACTIVE; retain |
| `_Archive\openu5` | 485.75 | `2485950ed6e0 / enhanced-mode` | clean; stash wip3 | KEEP — REFERENCE; retain |
| `_Archive\u5-ti89` | 2,071.37 | `non-Git` | local material | KEEP — REFERENCE; retain |
| `spp1-hw-package` | 3.32 | `non-Git` | local material | KEEP — REFERENCE; retain |
| `spp2-scratch` | 107.97 | `non-Git` | local material | ARCHIVE; retain pending verified archive |
| `ultima-v-native-parity` | 1,843.65 | `fc4a63aa6276 / parity/shared-preservation-1` | clean | KEEP — ACTIVE; retain |
| `ultima-v-native-rc1` | 2,589.45 | `a96656b23300 / release/v0.5.0-rc1` | clean | KEEP — REFERENCE; retain complete RC |
| `ultima-v-native-spp2` | 1,855.81 | `bf3480e3f4b5 / parity/shared-preservation-2` | clean | KEEP — ACTIVE; retain |
| `ultima-v-native-windows` | 2,059.13 | `e089f2c5fdc1 / windows/initial-port` | 43 untracked; 0 tracked edits | KEEP — ACTIVE; retain |
| `uvw-base` | 541.18 | `2c8de82176f1 / DETACHED` | 67 untracked; 0 tracked edits | KEEP — REFERENCE; retain |
| `uvw-dos-base` | 825.63 | `bf3480e3f4b5 / DETACHED` | 2 untracked; 0 tracked edits | KEEP — REFERENCE; retain |
| `uvw-dos-shops` | 1,867.73 | `00e3bd22d784 / windows/original-dos-shops` | clean | KEEP — ACTIVE; retain |
| `uvw-dos2-base` | 480.13 | `60e7c647bfa0 / DETACHED` | 3 untracked; 1 tracked edits | KEEP — REFERENCE; retain |
| `uvw-look-polish` | 2,437.07 | `9c98054782eb / windows/look-enhanced-polish` | clean | KEEP — ACTIVE; retain |
| `uvw-look-polish-evidence` | 138.71 | `non-Git` | local material | KEEP — REFERENCE; retain |
| `uvw-lp-base` | 328.41 | `00ae0410b39a / DETACHED` | 2 untracked; 0 tracked edits | KEEP — REFERENCE; retain |
| `uvw-original-dos` | 1,923.57 | `00ae0410b39a / windows/original-dos-interaction` | 1 untracked; 0 tracked edits | KEEP — ACTIVE; retain |
| `uvw-original-dos-evidence` | 2.77 | `non-Git` | local material | KEEP — REFERENCE; retain |
| `uvw-present` | 479.01 | `d689ef0148ed / windows/presentation-wip` | clean | KEEP — ACTIVE; retain |
| `uvw-shops-base` | 825.97 | `60e7c647bfa0 / DETACHED` | clean | KEEP — REFERENCE; retain |
| `uvw-spp1-base` | 601.36 | `e089f2c5fdc1 / DETACHED` | clean | KEEP — REFERENCE; retain |
| `uvw-spp2-base` | 826.84 | `fc4a63aa6276 / DETACHED` | clean | KEEP — REFERENCE; retain |

The initial primary status contained exactly the three `native/core/a4-final-hw-*.log`
files and `native/targets/tdeck/A4-FINAL-HW.md`. They remain byte-identical. The raw
census status additionally includes the newly created audit collector; this is an
audit addition, not pre-existing work.

`C:\Dev\ultima-v-native-windows` has 43 untracked build/fidelity/validation logs;
`uvw-base` has 67 untracked investigation scripts/reports/logs. Both are retained.
`uvw-dos-base` and `uvw-lp-base` each have two untracked probe logs.
`uvw-original-dos` has one untracked firmware-export log.
**`uvw-dos2-base` has the pre-existing unstaged two-line CMake edit and three
untracked files**, including `host_tests/tdeck_diff_probe_test.cpp`. No initial
staged changes were present in the scoped repositories. Full filenames are in the
raw Git snapshot and per-file census.

### Branches and cached remote relationships

No fetch/push was performed. These relationships use the retained local
remote-tracking refs, not a fresh statement about GitHub.

`main` equals cached `origin/main`; `enhanced-mode` equals
`enhanced/enhanced-mode`. The other nine native local branches have no upstream.
RC1 is **69 commits ahead of cached origin/main**, with no origin-only commits;
no cached origin RC1 branch exists. The preserve branch has unique work and the
parallel shops branch diverges; neither is an automatic deletion candidate.
All seven detached HEAD commits are reachable from retained branch refs, but that
does not preserve their untracked/ignored files. Details:
[detached-head reachability](inventory/detached-head-reachability.json).

| Native local branch | HEAD | Upstream | Ahead / behind cached origin/main | Unique / missing versus RC1 |
| --- | --- | --- | --- | --- |
| `enhanced-mode` | `2485950ed6e0` | `refs/remotes/enhanced/enhanced-mode` | 0 / 223 | 0 / 292 |
| `main` | `96a55ff9084f` | `refs/remotes/origin/main` | 0 / 0 | 0 / 69 |
| `parity/shared-preservation-1` | `fc4a63aa6276` | `(none)` | 43 / 0 | 0 / 26 |
| `parity/shared-preservation-2` | `bf3480e3f4b5` | `(none)` | 50 / 0 | 0 / 19 |
| `preserve/a4-parity1-wip` | `ff456642f565` | `(none)` | 1 / 62 | 1 / 131 |
| `release/v0.5.0-rc1` | `a96656b23300` | `(none)` | 69 / 0 | 0 / 0 |
| `windows/initial-port` | `e089f2c5fdc1` | `(none)` | 28 / 0 | 0 / 41 |
| `windows/look-enhanced-polish` | `9c98054782eb` | `(none)` | 64 / 0 | 0 / 5 |
| `windows/original-dos-interaction` | `00ae0410b39a` | `(none)` | 60 / 0 | 0 / 9 |
| `windows/original-dos-shops` | `00e3bd22d784` | `(none)` | 55 / 0 | 2 / 16 |
| `windows/presentation-wip` | `d689ef0148ed` | `(none)` | 17 / 0 | 0 / 52 |

`git fsck --connectivity-only --no-dangling` reports a pre-existing dangling
symbolic remote HEAD: `.git/refs/remotes/origin/HEAD` points to
`refs/remotes/origin/esp32-tdeck-port`, which is absent. `origin/main` and the RC1
branch resolve correctly. A separate `rev-list --objects --all --missing=print`
scan returned no missing reachable objects. **This is not a clean fsck result.**
An optional future `git remote set-head origin main` would repair only this local
symbolic ref, after confirming the intended remote default. No repair was made.

## RC1 protection and earlier packages

The historical reference is verified unchanged:

- Worktree: `C:\Dev\ultima-v-native-rc1`.
- Branch: `release/v0.5.0-rc1`.
- HEAD: `a96656b2330097c18c52b78a8840d3749c0e9854`; clean before/after.
- Windows ZIP: `native/targets/windows/dist/UltimaV-Native-v0.5.0-rc1-Windows-x64.zip`,
  **2,411,152 bytes**, SHA-256
  `97e2ae5201076d2f96e51e80063382f7116e39c52d3cbb337faf285e2db51e54`.
- Firmware: `native/targets/tdeck/dist/UltimaV-Native-v0.5.0-rc1-TDeck-Launcher.bin`,
  **1,075,088 bytes**, SHA-256
  `742dffcfc2675d02473e1566951af7ff36d743b205051aaac40d72f6893ec787`.

The retained external manifest identifies that same full source commit. The audit
verified ZIP CRC/whitelist, exact embedded Windows version/commit and AMD64 PE,
matching retained EXE/DLL, firmware ESP integrity/version/commit and matching
optimized app bytes. Final host and Windows logs still match the manifest's
hashes and record **212/212** and **38/38**. Relevant TypeScript and tooling
receipts still record **11/11** and **5/5**. Frozen and final paced/unpaced traces
retain matching hashes and bytes; each retained summary records 1,065 walks,
8,514 snapshots, zero differences, and 1,054 differing DOS positive-control walks.
These are verified historical receipts, not rerun host/Windows gameplay suites.
The five isolated release-tooling methods were rerun and passed.

RC1 inherits SPP1, SPP2, Original DOS interactions/shops, LOOK-POLISH1 and D-117 as
documented by the retained integration record. The separate shops record has
patch-equivalent inherited work, not simple whole-branch ancestry. Preserve the
parallel branch until its history/provenance is deliberately retired.

Protect the **whole RC checkout**, `build-rc1`, both firmware build trees, both
Windows build trees, all specialized fixtures, private packs, final manifest and
the complete `dist/v050-rc1-evidence/`, including
`attempt-18e464e3a928`, `preoptimization-reference.exe`, frozen reference traces,
optimization checkpoints and extraction-smoke data. The failed attempt's ZIP has
SHA-256 `4633c1fd…f373ce` and firmware `b2e3e396…600829`: their matching filenames
and sizes do not make them duplicates of the final packages.

Earlier packages remain required as follows; none is approved for deletion:

| Material | Retention purpose |
| --- | --- |
| `C:\Dev\spp1-hw-package\OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin` | Documented hardware rollback, earlier physical PASS user-reported; 1,072,912 B, SHA-256 `1aeecb70dec68141122e4aa88a9ce140cad87a617ff5668ec3eb8fa1d8b123c2` |
| SPP1 Windows test ZIP, source `96f5de63ab15` | Pre-SPP2 test comparison baseline; preserve package, original build and test receipts |
| SPP2 Windows test ZIP, source `0bfa8e3a9d2b` | Shared timing/interaction comparison; device acceptance not inferred |
| Initial Windows dev ZIP, source `e089f2c5fdc1` | Windows-port rollback/comparison baseline |
| ORIGDOS1 / ORIGDOS2 ZIPs, sources `e8cb8ba50ba5` / `caa20e729e61` | Staged DOS interface/shops historical comparisons; keep both |
| LOOK-POLISH1 ZIP, source `5b7f8bbc8713` | Accepted Windows presentation/signpost comparison; keep associated screenshots and traces |
| Main `build-a3-rc1-post` and `build-a4-rc5` packages | Historical release baselines explicitly protected by existing cleanup policy |
| Main `build-core/launcher` M5, Alpha1, Alpha1.1, Alpha2 images | Early historical provenance; acceptance/provenance varies, so retain pending individual review |
| Main BOOT2 and other RC5-named images | Distinct binaries/provenance; retain until historical role is reconciled |

The main untracked A4 hardware document says the `build-core` image
`9388b598…61efdd7` has pending hardware steps. Its RC5-style filename does **not**
identify the original RC5 image (`363a8fda…67210`, retained in `build-a4-rc5`), the
BOOT2 image (`35903e03…564b2`) or SPP1 (`1aeecb70…123c2`). Do not infer acceptance
from a filename, tag name, newer timestamp or another image's PASS.

Every catalog entry contains its full exact path and full SHA-256. Historical
package ZIP CRCs were checked when read; a catalog entry's retention decision
does not certify gameplay acceptance of every archived binary.

## Classification and repository organization findings

| File classification | Files | Logical MiB |
| --- | ---: | ---: |
| KEEP — ACTIVE | 61,347 | 9,699.83 |
| KEEP — REFERENCE | 103,662 | 13,960.83 |
| ARCHIVE | 2,249 | 107.97 |
| REGENERABLE | 14,042 | 912.61 |
| DUPLICATE | 4 | 59.66 |
| UNKNOWN | 37,827 | 3,398.14 |

The classification is conservative. A directory's class is a container policy;
its children may have different classes. `UNKNOWN` includes mixed compiler/static
library outputs, generated headers/configuration, snapshots and uncatalogued local
material whose exact regeneration inputs or historical value were not established.
These files stay in place. **UNKNOWN does not mean disposable.**

Specific findings:

1. Existing source organization is coherent: shared native core, per-platform
   targets, `native/tools`, extractor/reference code and established reverse-
   engineering notes. No source-layout move is warranted for this cleanup.
2. Build outputs dominate several source trees. The existing
   `native/tools/cleanup_builds.py` has explicit protected paths, backup gates and
   historical manifest entries. It was read, not executed in deletion mode. Its
   historical assertion that external release copies exist is not a verified
   current backup: `C:\Dev\openu5-release-artifacts` is absent from the root census.
3. `native/core/build-shops`, `build-real-arenas`, `build-dialogue`, `build-quests`,
   `build-gameplay`, `build-persistence`, `build-dungeon`, `build-tools` and
   `build-zig` must stay. CMake explicitly reads `flow.txt`, `helpers.txt`,
   `dialogue.bin`, `combat.txt`, `maps.txt` and `advanced-combat.txt` from these
   locations. Several contain baseline-source snapshots or toolchains.
4. Tracked native `.log` records and local RED/mutation/validation logs are formal
   evidence. Broad `*.log`/`build*` cleanup or ignore rules would hide useful work.
   The four primary untracked A4 files, 43 Windows logs and DOS-base probe work
   remain visible. No broad new ignores were added.
5. The tracked-file census found no `.exe`, `.dll`, `.o`, `.obj`, `.pyc`, `.zip`,
   `.vite*` or `CMakeFiles` paths. This scoped screen does not prove every tracked
   generated fixture is accidental; intentionally tracked fixtures stay unchanged.
6. `spp2-scratch` contains unique patch/mutation scripts, a pre-D111 dialogue
   binary, coverage JSON, RED results, a `.bak` test, comparison logs, a preserved
   SPP1 EXE and local SDL source. It is an **archive unit**, not a disposable temp
   directory. `uvw-base/audit-fw` similarly holds unique investigations.
7. Root C:\Dev has no loose files in the initial census. Loose project evidence
   is concentrated in native build/core folders and the four loose package/
   investigation/evidence roots, rather than arbitrary root documents.
8. Absolute dependencies include the TamaPoke w64devkit, Espressif SDK/toolchains,
   `spp2-scratch`'s SDL source override, cross-worktree firmware size baselines,
   mutation/patch scripts and private user-memory paths. Do not run patch scripts
   as part of cleanup. CMake caches embed source/build/compiler paths and should
   be regenerated after an approved move, not edited with search/replace.
9. RC1 `original` and `node_modules` junctions point through `uvw-look-polish`,
   which itself points into the primary checkout. Other worktrees also have
   chained dependencies. Moving an apparently retired source checkout can break
   RC1. All 54 links currently resolve. Root ACL observations are saved; no ACL
   change or write/move-lock probe was performed.

## Proposed structure and exact approval boundary

Use the [proposed directory tree](../../WORKSPACE_ORGANIZATION.md#proposed-organization).
Keep the primary and RC1 paths stable through acceptance. Create consolidated
locations for future worktrees/builds and durable historical copies only after
reviewing the exact action list. Retain project-owned tools and documentation in
their established source directories. No C:\Dev organizational folders were
created during this audit.

The following is the **proposal**, not actions performed. Full prerequisites and
exact byte sizes are in [approval-plan.csv](inventory/approval-plan.csv).
An archive/copy is not permission to remove its source. Any later removal must
be separately approved after hash/restore verification and dependency closure.
No branch deletion, repository deletion or Git history rewrite is proposed.

| Exact source path | Proposed action | Exact destination or retained authority |
| --- | --- | --- |
| `C:\Dev\OpenU5-Native\native\core\tools\__pycache__` | Delete cache directory | `` |
| `C:\Dev\OpenU5-Native\native\targets\tdeck\__pycache__` | Delete cache directory | `` |
| `C:\Dev\OpenU5-Native\native\tools\__pycache__` | Delete cache directory | `` |
| `C:\Dev\OpenU5-Native\re\tools\__pycache__` | Delete cache directory | `` |
| `C:\Dev\ultima-v-native-spp2\native\core\tools\spp1_differential\__pycache__` | Delete cache directory | `` |
| `C:\Dev\ultima-v-native-windows\native\targets\windows\build-ninja-Release\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` | Delete redundant SDL source archive | `C:\Dev\uvw-look-polish\native\targets\windows\build-pkg\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` |
| `C:\Dev\ultima-v-native-windows\native\targets\windows\build-release\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` | Delete redundant SDL source archive | `C:\Dev\uvw-look-polish\native\targets\windows\build-pkg\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` |
| `C:\Dev\uvw-look-polish\native\targets\windows\build-base\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` | Delete redundant SDL source archive | `C:\Dev\uvw-look-polish\native\targets\windows\build-pkg\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` |
| `C:\Dev\uvw-look-polish\native\targets\windows\build-look\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` | Delete redundant SDL source archive | `C:\Dev\uvw-look-polish\native\targets\windows\build-pkg\_deps\sdl3-subbuild\sdl3-populate-prefix\src\SDL3-3.4.16.tar.gz` |
| `C:\Dev\spp2-scratch` | Archive complete investigation; retain original until approved retirement | `C:\Dev\ultima-v-archive\spp2-scratch` |
| `C:\Dev\uvw-base\audit-fw` | Archive complete investigation; retain original until approved retirement | `C:\Dev\ultima-v-archive\audit-fw` |
| `C:\Dev\uvw-look-polish-evidence` | Copy complete reference package/evidence; keep original | `C:\Dev\openu5-release-artifacts\historical\uvw-look-polish-evidence` |
| `C:\Dev\uvw-original-dos-evidence` | Copy complete reference package/evidence; keep original | `C:\Dev\openu5-release-artifacts\historical\uvw-original-dos-evidence` |
| `C:\Dev\spp1-hw-package` | Copy complete reference package/evidence; keep original | `C:\Dev\openu5-release-artifacts\historical\spp1-hw-package` |
| `C:\Dev\ultima-v-native-rc1\native\targets\tdeck\dist` | Copy complete RC1 packages and evidence; keep original | `C:\Dev\openu5-release-artifacts\v0.5.0-rc1\tdeck` |
| `C:\Dev\ultima-v-native-rc1\native\targets\windows\dist` | Copy complete RC1 packages and evidence; keep original | `C:\Dev\openu5-release-artifacts\v0.5.0-rc1\windows` |
| `C:\Dev\uvw-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-base` |
| `C:\Dev\uvw-dos-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-dos-base` |
| `C:\Dev\uvw-dos2-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-dos2-base` |
| `C:\Dev\uvw-lp-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-lp-base` |
| `C:\Dev\uvw-shops-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-shops-base` |
| `C:\Dev\uvw-spp1-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-spp1-base` |
| `C:\Dev\uvw-spp2-base` | Archive full checkout for possible later retirement; no removal proposed yet | `C:\Dev\ultima-v-archive\worktrees\uvw-spp2-base` |
| `C:\Dev\ultima-v-native-parity` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\ultima-v-native-parity` |
| `C:\Dev\ultima-v-native-spp2` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\ultima-v-native-spp2` |
| `C:\Dev\ultima-v-native-windows` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\ultima-v-native-windows` |
| `C:\Dev\uvw-dos-shops` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\uvw-dos-shops` |
| `C:\Dev\uvw-look-polish` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\uvw-look-polish` |
| `C:\Dev\uvw-original-dos` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\uvw-original-dos` |
| `C:\Dev\uvw-present` | Optional later git worktree move; defer through RC acceptance | `C:\Dev\ultima-v-worktrees\uvw-present` |

No package, complete build tree, whole repository, original-data directory,
acceptance record or worktree is in the immediate deletion proposal. Historical
worktree archive rows are evaluations for later retirement, not an instruction
to unregister them. Future moves must use Git's worktree support, not filesystem
moves, and account for incoming junctions, relative layout, locks and ACLs.

## Disk recovery estimate

- **Completed recovery: 0 bytes.** Audit files add local storage.
- **Conservative approval-only candidate: 62,768,432 logical bytes (59.86 MiB)**:
  five verified Python caches (205,764 B), plus four SDL source archive copies
  (62,562,668 B). The retained offline SDL archive is the `build-pkg` path listed
  in every duplicate row, SHA-256
  `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`.
- **956,730,196 B (0.891 GiB) of identified compiler objects remain retained**.
  They map to existing source, compiler and exact command databases, but removing
  them would disturb preserved builds; this is not an immediate reclaim estimate.
- **3,824,515,931 B (3.562 GiB) is the raw hashed-duplicate upper bound**, not
  additive to the figures above and not safely reclaimable as a whole. It includes
  required fixture paths, private packs, active executables and independent
  comparison receipts. One 2,266,819-byte resource pack occurs in 355 physical
  paths; many are scenario fixtures. Hash equality does not erase their roles.
- Archiving on C: generally rearranges or adds data. Only later approved removal
  of independently verified retained copies could reclaim their measured bytes.
  Moving complete worktrees to another directory on C: recovers no disk space.

## Safe changes completed and validation

Added the permanent workspace guide, this report, read-only census/classification/
verification tools, path-dependency report and local detailed census. Added exactly
one narrow ignore entry, `/docs/workspace/CLEANUP1/inventory/`, with an explicit
retention comment. Existing ignored data was not cleaned or reclassified by Git.
Nothing was staged or committed.

Preservation verification passed **63/63 checks**. Every inventoried original file
remains accessible; sizes/timestamps are unchanged except the intended `.gitignore`
edit. Pre-existing untracked hashes match. All relevant HEADs, branch refs,
worktree registrations and stash lists match the census; all other worktree
statuses are unchanged. Primary status differs only by this audit's documentation/
tools and narrow ignore change. The original DOS-base edit remains in place and
has an additional read-only patch capture. Primary index remains unchanged.
All 54 reparse points still resolve to their original targets. The independent
archived OpenU5 repository's connectivity check passed; its stash remains intact.

RC1 packages and original evidence retain their exact hashes/provenance. Five
fresh release-tooling regression methods pass. `git diff --check` passes for the
tracked ignore edit. Full host and Windows runtime suites were not rerun because
no runtime/build/test source changed; their authoritative retained receipts were
verified instead. No hardware tests, firmware flashing, device access, main
integration, remote fetch, push or publication occurred. The separate dangling
remote-HEAD diagnostic remains unresolved and is not counted as a passing fsck.

## Unresolved items and follow-up decisions

1. Decide when RC1 physical acceptance can occur; preserve the SPP1 fallback and
   current RC context until then.
2. Decide which old checkouts still need interactive comparison. Seven detached
   HEADs are reachable, but their local data and cross-path dependencies prevent
   automatic retirement. `uvw-dos2-base` is actively modified.
3. Preserve or export the independent archived OpenU5 stash before any archive
   migration. Neither native Git history nor a source-only archive substitutes
   for that repository's stash, reflogs and private files.
4. Confirm historical image-to-acceptance mappings, especially RC5-named images
   and early Alpha packages. A4's untracked hardware checklist is still pending;
   do not upgrade it from another record's status.
5. Resolve mixed build material conservatively. The per-file UNKNOWN list totals
   about 3.32 GiB, mostly outputs/configuration without sufficient regeneration
   provenance. Inspect it before broadening the reclaim proposal.
6. Establish an independent backup destination and verify restoration. No durable
   external release archive was found at the legacy tool's expected C:\Dev path;
   backups elsewhere were not inventoried.
7. Optionally repair cached `origin/HEAD` after confirming the intended default.
   Keep this separate from source/branch cleanup; never prune refs as a shortcut.
8. Plan a separate path-parameterization change for tools tied to TamaPoke,
   Espressif, sibling checkouts and user memory. It is outside CLEANUP1's behavior-
   preserving documentation scope.

Stop point reached: the exact proposals are available for review. No higher-risk
cleanup action has been taken.
