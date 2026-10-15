# CLEANUP2 controlled workspace reorganization

15 October 2026. Completed within the preservation boundary. **No gameplay,
firmware, Windows functionality or Original/Enhanced behavior changed.** No
repository, worktree, release package, original asset or historical evidence was
moved or deleted. No history rewrite, remote operation or hardware test occurred.

## Before and after

CLEANUP1's existing inventory was reused; the 219,131-file census was **not rerun**.
All 17 scoped repository HEADs/branches matched its baseline at entry. The archived
`wip3` object and exact uncommitted DOS-base patch also matched. No unrelated work
was staged; the primary index was empty at entry and after each commit.

| Area | Before | After |
| --- | --- | --- |
| Primary checkout | `main` at `96a55ff9084f69acf776695a4fd1a5c5b15f16cb` | `main`, cleanup documentation/tool commits only |
| Native checkouts | Primary plus 15 registered linked worktrees | Same paths, registrations and non-primary HEADs/statuses |
| RC1 | `release/v0.5.0-rc1`, `a96656b2330097c18c52b78a8840d3749c0e9854` | Unchanged; packages and retained evidence hash-identical |
| Loose files directly in `C:\Dev` | 0 | 0 |
| Directories directly in `C:\Dev` | 31 | 34; all original roots retained |
| CLEANUP1 | Uncommitted summary/tools; local census | Durable files committed; local census retained and excluded |

New structure, containing only three instruction files:

```text
C:\Dev\ultima-v-builds\README.md
C:\Dev\ultima-v-archive\investigations\README.md
C:\Dev\openu5-release-artifacts\README.md
```

These are the adopted locations for **new** build intermediates, investigations
and release attempts. Existing output locations and script defaults remain intact.
The new release directory contains no historical packages and is **not a backup**.
`ultima-v-worktrees` and `ultima-v-tools` remain unimplemented proposals.

## Exact changes and commits

| Commit | Scope |
| --- | --- |
| `8fe5ffe4bae2038095f40b098ea1f40903ca7766` | Seven CLEANUP1 files: narrow inventory ignore, workspace guide, report, tool README and three audit tools |
| `85ff23ed51cb318b5d4edc499e9879e633c0be32` | Output conventions, contributor link, narrow CLEANUP2 local-evidence ignore, targeted verifier, exact manifests and approval plan |

This report is saved in the following closeout commit. Resolve that commit with
`git log -1 --format=%H -- docs/workspace/CLEANUP2/REPORT.md`; a commit cannot embed
its own hash. The final task response also records the closeout hash.

The sole relocation was
`docs/workspace/CLEANUP1/path-dependencies.txt` →
`docs/workspace/CLEANUP1/inventory/path-dependencies.txt`: **44,602 bytes**, SHA-256
`382fe4c4bdf29c04bdaeccf37e83f0795c8c9b19d89ed30f46a161e3ccbc8bc8`
before and after. It is a generated absolute-path listing. The report link was
updated; there were no tracked script/config consumers in 17 scoped repositories
or matching junction/worktree targets. Reversal is an ordinary same-directory
move back, provided the old filename is absent.

The exact ledger is [changes.csv](changes.csv); before/after relocation hashes are
in [relocations.csv](relocations.csv). [created-layout.csv](created-layout.csv)
records all four new directories and three README files, totaling **1,713 bytes**
of external file content, with verified hashes. [decisions.csv](decisions.csv)
records why remaining categories stayed in place. Repository changes affect only
`.gitignore`, `CONTRIBUTING.md`, the workspace guide and CLEANUP1/CLEANUP2 documents
and audit tools. No existing developer/build tool was relocated.

Manual review and credential-pattern scans found no credentials in the reviewed
CLEANUP1 candidates or local inventories; the scan is not a universal secret
detector. Large inventories, absolute-path dumps and Git snapshots remain local,
outside version control. Intended workspace paths in the durable guide/tools
document the audit scope. No game assets or private content were committed.

## Validation

- After phase A and after the layout/documentation group: **141/141** preservation
  comparisons passed. After the organization commit: **142/142**, including an
  additional primary-stash comparison.
- Each snapshot checked **774 selected preservation hashes**, **54 reparse
  targets**, **24 frozen inventory checksums**, Git status/branch/ref identities,
  worktree registrations, stash identities and exact DOS-base diffs. All original
  non-primary working states and the four pre-existing primary A4 files remain.
- RC1 package SHA-256 values remain `97e2ae5201076d2f96e51e80063382f7116e39c52d3cbb337faf285e2db51e54`
  (Windows ZIP) and `742dffcfc2675d02473e1566951af7ff36d743b205051aaac40d72f6893ec787`
  (Launcher firmware). Full retained RC1 evidence selected by CLEANUP1 also matches.
- Fresh RC1 release-tooling tests: **5 passed**. Cleanup safety tests: **15 passed,
  1 skipped** because Windows denied test symlink creation. Five simulated
  preservation-loss cases were rejected by the new verifier without input mutation.
- Audit Python syntax, collector `--help`, narrow ignore rules, relocation/new-file
  hashes and staged whitespace checks passed. New documentation links resolve.
- No missing objects reachable from retained refs in either repository. Archived
  OpenU5 connectivity passes. Native `git fsck` still exits 2 for the pre-existing
  invalid `origin/HEAD` target; this is **not** reported as a clean fsck.

[validation-summary.json](validation-summary.json) records the scope and test-log
hashes. Detailed snapshots and logs are retained in ignored `local/`; CLEANUP1's
original inventory and checksum list were not overwritten. Full runtime suites
were not rerun for documentation/organization-only changes. Historical RC1
receipts remain evidence of their original runs. Physical RC1 acceptance remains
**NOT RUN**.

## Recovery, approvals and migration hazards

**Actual disk-space recovery: 0 bytes.** No existing file was deleted; the sole
move stayed on C:. Documentation, snapshots and Git commits add storage. Allocated
NTFS growth was not measured. Root clutter could not safely be reduced: every
existing candidate was active, historical evidence, unique work or dependency-bound.

[approvals.csv](approvals.csv) carries forward all **30 exact candidates**, their
destinations and prerequisites. Each source still exists. Its size estimates are
CLEANUP1 logical bytes, not refreshed deletion estimates. Explicit approval is
still required before these actions:

| Candidates | Next approval and prerequisites |
| --- | --- |
| Five cache directories and four duplicate SDL archives | Approve exact paths; remeasure only those candidates, verify adjacent source/retained archive hashes, reject reparse points and confirm no active use. Earlier estimate: 62,768,432 bytes; not reclaimed. |
| `spp2-scratch` and `uvw-base/audit-fw` | Approve a complete archive/restore plan; preserve unique scripts and before-state bytes, resolve absolute-path consumers before relocation. |
| Three historical package/evidence roots and two RC1 `dist` roots | Approve complete verified copies with provenance and restoration checks; retain sources and failed attempts. Establish an independent backup destination. |
| Seven historical checkout archive candidates | Approve full capture including ignored/untracked files, diffs, refs and detached HEADs; restoration first. Retirement/removal requires separate approval. |
| Seven active checkout move proposals | Defer through acceptance; resolve junctions/absolute paths and fresh-build validation. No repository/worktree moves are part of CLEANUP2. |

Distinct RC5 images, acceptance records, SPP1 rollback, archived stash, active
DOS-base work and original assets remain protected. Existing CMake fixture paths,
embedded CMake caches, TamaPoke toolchain paths and sibling-checkout dependencies
prevent cosmetic moves. The Windows wrapper supports a build directory but keeps
packages in target-local `dist`; output-default changes need a separate change.
Do not infer backup readiness from the newly created release-directory name or
from older cleanup-tool comments. Repairing the cached remote HEAD, parameterizing
tools and reconciling image-to-acceptance history remain separate follow-ups.
