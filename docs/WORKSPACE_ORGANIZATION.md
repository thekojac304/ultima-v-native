# Ultima V Native workspace organization

Preserve source, private game data, uncommitted work, release bytes and acceptance
evidence before reclaiming space. Ignored files can be the only retained copy.
The [CLEANUP1 audit](workspace/CLEANUP1/REPORT.md) records the Windows workspace
inventory, proposed actions and approval boundary as of 15 October 2026.

## Current working paths

`C:\Dev\OpenU5-Native` is the primary repository and holds the shared Git object
store. `C:\Dev\ultima-v-native-rc1` is the registered RC checkout on
`release/v0.5.0-rc1`. Other sibling checkouts belong to that same repository only
when `git worktree list --porcelain` establishes the relationship. A `.git` file
in a linked checkout is essential metadata.

Keep these locations stable during RC acceptance. Several junctions share the
primary checkout's dependencies and original data; CMake caches and investigation
scripts also embed absolute paths. Never move a linked checkout with Explorer or
ordinary filesystem moves. An approved future move must use Git's worktree
operation, verify its metadata and junctions, and rebuild into a fresh build tree.

## Adopted locations for new work (CLEANUP2)

```text
C:\Dev\
  OpenU5-Native\                  primary repository, stable path
    docs\WORKSPACE_ORGANIZATION.md
    docs\workspace\CLEANUP1\      frozen audit summary, tools and local inventory
    docs\workspace\CLEANUP2\      reorganization report and targeted verifier
  ultima-v-native-rc1\            retain through RC acceptance
  ultima-v-worktrees\             future active checkouts, after approval
  ultima-v-builds\                new build intermediates, grouped by checkout/task
  openu5-release-artifacts\       new release attempts and their evidence
  ultima-v-archive\
    investigations\              new temporary investigations, grouped by task
  ultima-v-tools\                 tools genuinely shared between projects
```

Only `ultima-v-builds`, `openu5-release-artifacts` and
`ultima-v-archive/investigations` were created, with README instructions. No old
build, package, investigation, repository or worktree was migrated. The
`ultima-v-worktrees` and `ultima-v-tools` paths remain proposals and do not exist.
The release directory starts with instructions only: **it is not a release backup**.
RC1, earlier packages and historical evidence remain at their audited paths.
See the [CLEANUP2 report](workspace/CLEANUP2/REPORT.md) for exact receipts.

Keep project scripts
under `native/tools/` and platform tools under their existing target directories.
Keep source documentation in `docs/` and established platform/reference notes in
their current locations. Avoid cosmetic source moves.

New build commands should accept an output path and derive source paths from the
checkout. Existing `build-shops`, `build-real-arenas`, `build-dialogue`, other
specialized fixture workspaces, private assets and toolchain trees remain in
place until their consumers are deliberately migrated. Do not treat them as
ordinary caches.

Use these naming conventions for new work:

| Purpose | Location under `C:\Dev` | Required context |
| --- | --- | --- |
| Build intermediates | `ultima-v-builds/<checkout>/<YYYYMMDD-task-attempt>/<host-or-platform>/` | Source checkout, full commit, configuration, toolchain and exact commands |
| Investigation | `ultima-v-archive/investigations/<YYYYMMDD-task-attempt>/` | Question, source commit/dirty state, commands, logs, findings and retention decision |
| Release attempt | `openu5-release-artifacts/<version>/<source-commit-12>/<attempt>/` | Packages, full source commit, SHA-256 manifest, configuration, licenses, test receipts and acceptance state |

Choose a fresh task/attempt directory; never overwrite an earlier attempt. Temporary
work is not automatically disposable. Promote useful scripts to `native/tools/`
and findings to the appropriate existing documentation location after review.
Keep private assets out of Git and retain evidence until its retirement is approved.
Do not add broad `*.log` or `build*` ignore rules.

For a **new ordinary host build**, CMake already accepts a separate output path:

```powershell
$sourceRoot = 'C:\Dev\OpenU5-Native'
$buildOutput = 'C:\Dev\ultima-v-builds\OpenU5-Native\YYYYMMDD-task-attempt\host'
cmake -S "$sourceRoot\native\core" -B $buildOutput -DCMAKE_BUILD_TYPE=Release
cmake --build $buildOutput --config Release
```

Replace the task/attempt placeholder before use. These are output-location
examples, not a new toolchain setup or a validation claim. Specialized fixtures
still come from their existing source-tree paths. Do not move CMake caches.
ESP-IDF supports `idf.py -B <absolute-new-output> build` from its source target
directory, but its existing configuration/build workflow remains authoritative.
The RC1 Windows wrapper accepts `-BuildDir` for intermediates and still writes
packages to its target-local `dist`; it has no configurable package destination.
Use existing release tools there, then separately verify any archival copy. A
future default/output-parameter change needs its own review and validation;
CLEANUP2 changes no build-script defaults.

## Release and evidence retention

Retain each release package with its source commit, SHA-256 manifest, build
configuration, licenses, test logs and acceptance status. Preserve failed attempts
and frozen reference traces alongside successful receipts. Keep hardware results
distinct from desktop tests; RC1 physical acceptance remains pending.

The SPP1 firmware package at `C:\Dev\spp1-hw-package` is the current documented
hardware rollback. Earlier Alpha packages, Windows packages and baseline checkouts
remain comparison material until their retirement is explicitly approved.
Original game files and derived private packs stay local and outside Git.

Before an approved archive/removal, capture unstaged and staged diffs, all untracked
and ignored material, refs and detached HEADs; make a restorable archive and verify
its hashes and restoration. A Git bundle alone omits uncommitted files, ignored
assets and reflog-only objects. Check locks, permissions, reparse targets and
dependencies before any move. Do not recursively follow junctions while copying
or deleting. Recheck status, worktree registrations, refs and release hashes after
the operation. `git clean -fdx`, hard resets, history rewrites and blanket build
deletion are inappropriate for this workspace.

The narrowly ignored CLEANUP1 `inventory/` directory is retained local evidence;
it is not backed up by Git. Preserve it separately before future cleanup.
The same applies to CLEANUP2's narrowly ignored `local/` snapshots and test logs.
