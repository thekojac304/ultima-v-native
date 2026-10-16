# CLEANUP2 supporting records

This directory holds the controlled reorganization report and small, reviewable
manifests. Detailed machine snapshots and test logs live in `local/`, narrowly
ignored by Git and retained as local preservation evidence. They are not a backup.

`verify_workspace.py snapshot LABEL` records current Git/worktree/stash state,
reparse targets and selected preservation hashes from CLEANUP1's existing census.
It does not walk the workspace or change the census. `compare LABEL LABEL`
compares those records, allowing only this cleanup's primary documentation/tools
and the expected main-branch commits. It refuses other changes. Run with
`python -B docs/workspace/CLEANUP2/verify_workspace.py ...` on this Windows workspace.

The CLEANUP1 tools retain their original audit-time assumptions. Do not rerun
their classifier or verifier over the frozen `CLEANUP1/inventory/`: they overwrite
derived receipts and their primary HEAD/ref assumptions predate the cleanup commits.
The collector requires a new output directory for a future separately scoped audit.
