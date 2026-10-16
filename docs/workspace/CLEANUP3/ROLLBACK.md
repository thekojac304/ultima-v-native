# CLEANUP3 rollback and batch gates

Status: all 25 moves are complete. This is a preserved inverse recovery procedure,
not an instruction to move anything during closeout. The original prerequisites
below governed the completed migration. Preserve all copies and current work.
The newer primary recovery is finish-20261016T002344Z/primary-recovery; its manifest
is primary-files.json in that same attempt directory. Closeout rehashed this copy
and all 25 original recovery roots. Save the authorized closeout commit and any
subsequent work before comparing with the earlier pre-closeout main baseline.

All 25 source/destination pairs are in mapping.csv. No destination may exist before its move.
No live root move is allowed until backup-complete.json, recovery-restore.json and rehearsal.json exist.
Every physical file is SHA-256 checked in recovery; links are recorded, never followed or copied as live links.
Recovery is independent physical storage on the same C: volume, not a device-loss backup.

## Planned sequence

1. Verify existing CLEANUP1/CLEANUP2 hashes/Git states (cleanup3-entry snapshot).
2. Record mapping, current path consumers, link graph, Git refs, diffs, status, ignored lists, reflogs and stashes.
3. Rehearse Git relocation and the inverse rollback in an isolated repository.
4. Copy all roots, including .git, ignored/untracked data, original assets and frozen inventories.
5. Independently hash every recovery file; restore archived Git/stash and DOS-base working files and verify.
6. Move artifacts, archives, CLEANUP2 convention roots, and appropriate builds in separate batches.
7. Use git worktree move for each linked checkout; keep primary stable during this batch.
8. Move primary with a same-volume rename and immediately run git worktree repair from its new path.
9. Recreate only existing shared-data junctions with translated real targets, after each root move.
10. Verify complete inventories/hashes and Git after each batch, then update operational path consumers.
11. Preserve old generated caches and receipts verbatim; build afresh into builds/cleanup3-validation.
12. Check all links, Git refs/stashes/status, critical hashes, current path consumers, tooling/host/Windows tests.

## Inverse rollback (without discarding edits)

Stop writers and save any work produced after migration. Do not reset, clean, prune or overwrite existing roots.
Read moves/*.json to identify precisely which moves actually completed, then reverse in this order:

- Save the current operational path edits as patches plus physical copies. Their before-bytes are in recovery.
- If source moved: rename C:\Dev\U5Native\source back to C:\Dev\OpenU5-Native only when old path is absent.
  Run git -C C:\Dev\OpenU5-Native worktree repair with all current linked worktree paths.
- In reverse linked-worktree move order, run git -C C:\Dev\OpenU5-Native worktree move NEW OLD.
- Reverse the completed non-Git/archive moves with same-volume renames, refusing existing old destinations.
- Recreate each recorded junction at the appropriate old location with its exact pre-migration target from manifests.
  Remove only the verified junction itself, never recursively remove its target.
- Compare all 17 repositories with git-before.json, including binary diffs, refs, stash identities and ignored lists.
  Verify every physical file against manifests and all CLEANUP1 selected hashes.

## Recovery after a failed rename or unavailable live data

Keep all originals and partial destinations. Do not overwrite or delete them. Use recovery/NN-root as a separate
physical restore input; verify against manifests/NN-root.json. These copies omit live junction entries; recreate
links from the manifest. The full native .git directory includes objects, refs, reflogs, index, stash and worktree
administration. Restore the complete group to a separate location, run git worktree repair, verify all baselines,
and only then switch to it. A source-only checkout or Git bundle is not a replacement for these copies.

rehearsal.json records forward and reverse Git moves with uncommitted and untracked content.
recovery-restore.json records an actual restore of the independent archived repository (including wip3)
and the DOS-base working files. Recovery copies are retained; no automated destructive rollback is supplied.
