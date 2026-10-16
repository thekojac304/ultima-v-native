# Ultima V Native workspace organization

CLEANUP3 consolidated the Ultima V Native workspace under `C:\Dev\U5Native`.
The [migration report](workspace/CLEANUP3/REPORT.md) records actual moves,
preservation checks, fresh builds and recovery instructions. CLEANUP1/CLEANUP2
remain frozen historical audits; their absolute paths describe the former layout.

## Current paths

```text
C:\Dev\U5Native\
  source\                         primary repository and common .git object store
    native\tools\                 current project tools
    docs\workspace\CLEANUP1\      frozen census and preservation evidence
    docs\workspace\CLEANUP2\      frozen follow-up evidence
    docs\workspace\CLEANUP3\      migration report, mapping and verification tools
  worktrees\                      all 15 registered linked checkouts
    ultima-v-native-rc1\           verified RC1 source, builds, packages and evidence
    uvw-dos2-base\                 original uncommitted DOS-base work
    ...
  builds\                         fresh outputs grouped by checkout/task/attempt
  releases\
    historical\                   SPP1 hardware rollback and DOS/LOOK evidence
    legacy-layout\                preserved CLEANUP2 release-directory instructions
  archive\
    openu5\                       independent archived repository, including wip3
    u5-ti89\                      complete historical TI project and tools
    investigations\spp2-scratch\  complete historical investigation
    legacy-layout\                preserved CLEANUP2 archive instructions
    CLEANUP3\                     full recovery copies, inventories and receipts
  tools\                          workspace entry points
```

The exact 25 source/destination pairs are in
[mapping.csv](workspace/CLEANUP3/mapping.csv). The 23 standalone Ultima V roots
were removed from the top level of `C:\Dev`. The shared `_Archive` parent remains
for unrelated projects; its two Ultima V children moved into this workspace.
There are no compatibility junctions at the old top-level paths.

Keep Git metadata with every checkout. Linked worktrees were moved with
`git worktree move`; relocation of the primary object store was followed by
`git worktree repair`. Use those operations for future changes, with verified
recovery first. Do not rename linked checkouts with Explorer, prune refs, rewrite
history, reset working files or run `git clean -fdx` as an organization shortcut.

## Builds and tools

Historical build/fixture trees remain inside their complete relocated checkouts.
Their relative paths are required by tests and private fixtures. Every original
file and generated cache was preserved. Old CMake/ESP-IDF caches embed old absolute
paths and are retired build snapshots: do not configure or build in them.
Do not edit their caches or logs to make historical evidence appear current.

Create a fresh output for every new attempt. For example:

```powershell
Set-Location C:\Dev\U5Native
.\tools\Build-U5Native.ps1 -Checkout source -Target Host -Test
.\tools\Build-U5Native.ps1 -Checkout ultima-v-native-rc1 -Target Host -DeveloperTools -Test
```

The workspace helper derives paths from its location and refuses an existing
CMake cache. Host builds retain the existing external TamaPoke w64devkit dependency;
override it with `-HostToolBin` or `U5_W64_BIN`. TamaPoke and shared Espressif/Visual
Studio installations are not moved as part of this project consolidation.
Windows builds use the checkout's established `native/targets/windows/tools/`
wrapper with a fresh `-BuildDir`. The validated offline RC1 Windows command and
its retained SDL source are in the CLEANUP3 test receipts.

CMake also supports explicit source/output paths:

```powershell
$sourceRoot = 'C:\Dev\U5Native\source'
$buildOutput = 'C:\Dev\U5Native\builds\source\YYYYMMDD-task-attempt\host'
cmake -S "$sourceRoot\native\core" -B $buildOutput -DCMAKE_BUILD_TYPE=Release
cmake --build $buildOutput --config Release
```

Choose a real task/attempt name and configure the established toolchain first.
ESP-IDF likewise requires a fresh `idf.py -B <new-output> build` from the source
target. Firmware hardware acceptance remains deferred. Windows packaging still
uses target-local `dist`; retain packages, provenance, test logs and failed
attempts together. Repackaging RC1 is not part of the migration.

Use the current `source/native/tools/cleanup_builds.py`, whose default backup
location is `C:\Dev\U5Native\releases\backups`. Existing protection/approval gates
still apply; this migration authorizes no build deletion. Historical cleanup-tool
copies and one-shot mutation/probe scripts retain their original bytes and paths
for provenance. They are archived tools, not current entry points. The dependency
ledger classifies them explicitly. Original CLEANUP1/CLEANUP2 verifiers retain
historical path assumptions. CLEANUP3's full-inventory verifier is a frozen
pre-closeout-commit audit; use the separate read-only closeout checks and the
recorded receipts to review completion after the authorized documentation commit.

New investigation output belongs in `archive/investigations/<date-task-attempt>/`.
New releases belong in `releases/<version>/<commit>/<attempt>/`, including hashes,
configuration, licenses, test receipts and acceptance state. Temporary output and
old builds are not disposable without reviewing their contents and dependencies.

## Preservation and recovery

The full RC1 checkout remains on `release/v0.5.0-rc1` at
`a96656b2330097c18c52b78a8840d3749c0e9854`. Its packages/evidence and original game
assets retain their bytes. SPP1's rollback package is now in
`releases/historical/spp1-hw-package`. The DOS-base edit/probe remain in
`worktrees/uvw-dos2-base`; the independent OpenU5 `wip3` stash remains in
`archive/openu5`.

`archive/CLEANUP3/recovery` contains independently hashed physical copies,
including both Git stores, ignored/untracked files and all original assets.
Junctions are captured as metadata, never traversed during copying. Restore them
from the manifests; a Git bundle alone cannot restore this workspace. Recovery
copies share C: with live data and do not protect against drive loss. Retain them
until a separately verified backup is established. See
[ROLLBACK.md](workspace/CLEANUP3/ROLLBACK.md) for the rehearsed inverse migration.

Do not overwrite frozen inventories, broaden ignore patterns, infer physical
acceptance from another package, or publish private game data. Existing shared
asset/dependency junctions now point directly into U5Native and are checked for
resolution. Historical receipts may legitimately contain former absolute paths.
Open the project and terminals at `C:\Dev\U5Native\source`. Any saved editor or
Codex project entry still naming the former root must be reselected there.
The current source path and all 16 Git registrations have been independently verified.


## CLEANUP3 completion

The final source relocation is complete. All 25 mapped roots are at their destinations;
23 standalone project roots were consolidated and no old project root remains.
The interrupted post-relocation run passed 202 primary-host, 212 RC1-host and 38
Windows tests. Its initial firmware attempt failed in Xtensa GCC at six jobs.
The user resumed that same build at two jobs successfully; closeout independently
validated the finished image, source/ELF identity, app budget and an incremental
Ninja run with no source recompilation. The new image remains in
`builds/cleanup3-finish-20261016T002344Z/rc1-tdeck`; existing RC1 releases are unchanged.
See the report for distinct failure/retry receipts and hashes. Hardware remains deferred.

The migration and offline finisher are completed one-time procedures retained as
historical tools. Do not rerun them or move directories again for this closeout.
Preserve both `archive/CLEANUP3/recovery` and the newer primary recovery in
`archive/CLEANUP3/finish-20261016T002344Z/primary-recovery`, together with all logs.
