# v0.5.0 RC1 progress and final gate receipts

- Gates 0–1 PASS: integrated ancestry and preserved original checkouts; baseline host 210/210 after the documented LF setup correction; Windows 38/38; ESP-IDF 6.1 app 1,096,432 bytes.
- Gate 2 PASS: simple fixed-handheld capability isolation recovered 21,344 bytes. Static RAM/IRAM unchanged; no retained compiler frame grew. Paced/unpaced traces match exactly on 1,065 walks / 8,514 snapshots each; DOS positive control differs in 1,054 walks. Eight boundary mutants killed and restored tests pass. Further optimization stopped.
- Gate 3 PASS: Windows 38/38 after optimization; Original/Enhanced presentation and all inherited gameplay remain protected.
- Gate 4 PASS: independent review found and resolved README encoding and exact RC version verification defects. Four provenance regression tests pass; the prefix-bug mutant was killed. Full implementation host suite 212/212 and relevant TypeScript 11/11 pass.
- Gates 5–6 receipts are generated after the final source commit: native/targets/tdeck/dist/v050-rc1-manifest.json. It verifies matching clean-source artifacts, final full suites, exact differential, mutation results, firmware integrity/budget and fresh-extraction smoke. Detailed logs remain in native/targets/tdeck/dist/v050-rc1-evidence.
- Integration, known preservation limits and all commands/results: docs/V050_INTEGRATION_RC1.md. Focused installation/backup/rollback and 20–30 minute acceptance: docs/V050_RC1_HARDWARE_CHECKLIST.md.
- Branch release/v0.5.0-rc1; worktree C:\Dev\ultima-v-native-rc1. No original development worktree or known-good package was replaced. Nothing pushed, published, merged or flashed.
- Remaining human gate: new RC physical acceptance, including SPP2, shared Look/rows, device input/audio/performance and real heap/stack observations. Earlier SPP1 physical PASS is user-reported.
