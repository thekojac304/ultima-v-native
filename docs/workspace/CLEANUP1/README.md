# CLEANUP1 audit tools and retained local evidence

The [report](REPORT.md) and [workspace guide](../../WORKSPACE_ORGANIZATION.md)
contain the durable conclusions. The three Python tools are retained audit source;
they intentionally describe the Windows workspace audited on 15 October 2026.
Their `C:/Dev` paths identify that scope, not a portable project setup requirement.
They contain no credentials or original game data.

`inventory/` contains large generated CSV/JSON files, absolute machine paths,
Git snapshots, patch captures and validation receipts. These are deliberately
excluded from version control and retained on disk. The generated
`path-dependencies.txt` listing also belongs there. They are preservation evidence,
not disposable output, and are not backed up by these commits.

The collector requires a new `--output` directory and does not alter its inputs.
The classifier and verifier use the adjacent inventory and overwrite derived
files there. Do not rerun them against the frozen audit: copy the complete input
set to a separately scoped investigation first and adapt paths/assumptions.
The verifier expects the pre-cleanup Git HEAD/refs, so later documentation commits
will legitimately invalidate those historical equality checks. CLEANUP2 provides
a targeted follow-up verifier using the existing inventory without changing it.

Syntax can be checked without bytecode or audit execution:

```powershell
python -B -c "import ast,pathlib; [ast.parse(p.read_text(encoding='utf-8')) for p in pathlib.Path('docs/workspace/CLEANUP1').glob('*.py')]"
```
