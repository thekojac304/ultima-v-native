# CLEANUP3 completed migration evidence and tools

All 25 mapped moves are complete, including the primary at C:\Dev\U5Native\source.
REPORT.md is the final account; mapping.csv is the original plan and actual-moves.csv
is the 25-receipt execution ledger. detailed-mapping.csv retains baseline Git/file
counts and now identifies every actual destination and the relocated common store.
Primary baseline HEAD/counts precede the authorized closeout documentation commit.

Large inventories, binary patches, ignored/private content, recovery copies and raw
logs remain local at C:\Dev\U5Native\archive\CLEANUP3. Do not stage those data.
Original failed and blocked checkpoints remain historical records. Latest closeout
receipts are in closeout-20261016T011631Z; the interrupted post-relocation test logs
are in finish-20261016T002344Z/tests. Firmware retry evidence does not replace the
original GCC failure. Read ROLLBACK.md before any separately authorized recovery.

The one-time migrate.py and tools/Finish-Primary-Migration.py are retained for review
and provenance. Both have already performed their migration work. DO NOT RERUN THEM.
validate-builds.ps1 is the original full-validation procedure, not required to repeat
completed builds. tools/Build-U5Native.ps1 is a review copy of the canonical helper
at C:\Dev\U5Native\tools; it creates fresh outputs rather than reusing old caches.

Read-only closeout checks (Python 3.14, from the relocated repository):

    python -B docs/workspace/CLEANUP3/closeout_checks.py firmware --output NEW.json
    python -B docs/workspace/CLEANUP3/closeout_checks.py receipts --output NEW.json
    python -B docs/workspace/CLEANUP3/closeout_checks.py recovery --output NEW.json
    python -B docs/workspace/CLEANUP3/closeout_checks.py paths --output NEW.json
    python -B docs/workspace/CLEANUP3/closeout_checks.py git --output NEW.json

Choose a different output for each command; existing receipts are never overwritten.
The git check requires the retained closeout-staging.json receipt and the exact final
closeout commit; later authorized development will legitimately invalidate it.
The recovery check hashes all 26 physical copies without following captured junctions.
The firmware check inspects the current completed build and runs Ninja dry runs only.

verify_migration.py performed the strict original 219,227-file and 17-Git-baseline
audit successfully after relocation and before the documentation commit. It purposely
requires original main HEAD/refs/index/status; after the closeout commit it no longer
matches. Review its frozen final-relocated-closeout-20261016T011631Z.json receipt,
then use the commit-aware git check to account for the single authorized advance.
restore_drift_metadata.py was used only for 24 explicitly recorded same-byte fixture
outputs whose timestamps changed during validation; output copies and receipts remain.
No content was rewritten by that metadata restoration.
