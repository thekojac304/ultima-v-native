# v0.5.0 RC1 progress

- Phase 0 / Gate 0 PASS: release/v0.5.0-rc1 in C:\Dev\ultima-v-native-rc1, based on 9c98054782eb9b62eefbb70718535991e0dede98.
- All 15 pre-existing worktrees retain their original HEAD and porcelain status. Snapshot: docs/v050-rc1-evidence/preexisting-worktrees.json.
- main, windows/initial-port, SPP1, SPP2, windows/original-dos-interaction and packaged LOOK-POLISH1 code 5b7f8bbc are ancestors. ORIG-DOS2 shop work is present via the original-dos-interaction line; its separate historical branch is not an ancestor.
- Phase 1 in progress: fresh GCC host build/serial test run, Windows and ESP-IDF baseline configured. Missing private build-shops/assets.inc in initial host setup was supplied from the preserved latest development checkout; no source fix required.
- No optimization applied yet. Remaining: finish clean baselines, measure/isolate handheld capabilities, differential and mutation checks, independent review, final clean-commit builds, packaging and hardware handoff.
- The sandbox launcher fails with HRESULT 0x80070020. Read/build commands use the approved execution fallback. No publication, push, merge or device flash.
- Evidence logs: native/targets/tdeck/dist/v050-rc1-evidence (ignored; preserve locally).
