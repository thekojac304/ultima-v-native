# v0.5.0 RC1 independent review

Reviewer: separate read-only subagent `rc1_independent_review`, baseline `9c980547` through `c1453fc`, followed by a second pass over the working-tree fixes. Review completed 2026-10-15. No unresolved actionable findings.

## Confirmed defects and resolutions

1. **README UTF-8 corruption introduced by RC editing.** Windows Python defaulted to cp1252, so edits damaged UTF-8 punctuation in the two existing target READMEs. Restored their original UTF-8 bytes from the baseline; baseline-to-final diff now contains only the intended RC note. All RC helpers explicitly read UTF-8. Runtime diff inspection confirmed no encoding changes in gameplay/application sources.
2. **Windows RC version-prefix acceptance.** The first manifest verifier's substring check accepted `rc10` or `rc1-dev` as `rc1`. It now requires exactly one Version field equal to `0.5.0-rc1`, exact clean source provenance, and a NUL-terminated RC version in the executable. Four regression test methods cover valid identity, other versions, dirty state and wrong commits. The reviewer independently reran them successfully. A valid mutant restoring prefix acceptance was killed; restored helper passes all four tests.

## Verified scope

- Fixed capability consistently applied to 101 shared firmware translation units and absent from 98 Windows shared translation units; no PlatformProfile layout change or mixed definitions.
- No confirmed runtime isolation, Original/Enhanced, shared-preservation, save-format, licensing, or packaging defect.
- Paced and unpaced trace hashes independently matched the frozen reference and candidate. The positive DOS control is different.
- Firmware reduction 21,344 bytes; RAM/IRAM sections unchanged; no retained compiler stack frame increase. Compiler frames and hardware high-water marks are correctly distinguished.
- Developer tools and shared gameplay remain compiled. No tests or warnings were disabled.

Final post-commit builds, package extraction/smoke and artifact manifests are independently checked by the RC tooling after this review. Physical T-Deck acceptance remains the user's task. The reviewer did not edit source, build firmware, mutate code, commit, publish, or flash.

## Post-package smoke correction

Fresh extraction caught a runner-only setup defect after commit `18e464e`: the four gameplay scripts require `--developer`, as their CTest declarations explicitly state, and the first package smoke omitted it. Missing assets, portable boot and standalone preparation of all three matching packs already passed; the game rejected the unprivileged cheat setup. The helper now supplies the exact scenario opt-ins, plain portable boot remains unchanged, and a new regression method derives expected options from CMake (five total helper methods PASS). A valid omitted-flag mutant is rejected. The corrected full ZIP smoke passed all five scenarios before the new clean-commit rebuild. The independent reviewer audited this limited correction and independently reran all five tests: no new actionable finding. Previous receipts, failed smoke and both first-attempt packages are retained in the local attempt archive. Runtime C++ code is unchanged.
