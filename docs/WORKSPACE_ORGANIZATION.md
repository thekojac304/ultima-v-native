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

## Proposed organization

```text
C:\Dev\
  OpenU5-Native\                  primary repository, stable path
    docs\WORKSPACE_ORGANIZATION.md
    docs\workspace\CLEANUP1\      audit summary and collector
  ultima-v-native-rc1\            retain through RC acceptance
  ultima-v-worktrees\             future active checkouts, after approval
  ultima-v-builds\                new disposable build intermediates
  openu5-release-artifacts\       durable release and rollback artifacts
    v0.5.0-rc1\                   packages, manifest, evidence together
    historical\                  earlier packages and their provenance
    backups\                     verified preservation copies
  ultima-v-archive\               retired investigations and checkout snapshots
  ultima-v-tools\                 tools genuinely shared between projects
```

This tree is a proposal, not a migration already performed. Keep project scripts
under `native/tools/` and platform tools under their existing target directories.
Keep source documentation in `docs/` and established platform/reference notes in
their current locations. Avoid cosmetic source moves.

New build commands should accept an output path and derive source paths from the
checkout. Existing `build-shops`, `build-real-arenas`, `build-dialogue`, other
specialized fixture workspaces, private assets and toolchain trees remain in
place until their consumers are deliberately migrated. Do not treat them as
ordinary caches.

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
