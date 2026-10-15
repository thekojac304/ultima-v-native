# LOOK-POLISH1 T-Deck differential

The probe and classifier behind the T-Deck differential in
`docs/WINDOWS_LOOK_ENHANCED_POLISH.md` §7. The probe is SPP2's (`../spp2_differential/`), with
two changes:

- **every walk prints LOOK2.DAT's phrases.** The probe sets the harness's `g_look_text`, so the
  host fixture's (L)ook reads the pack's tables (`HostTestFixture::look_text_from_pack`) instead
  of answering "something" for every tile. Without it the Look fix (D-117) is invisible.
- **walk J** reaches what this batch changes on the device: Look at four of Britain's sign posts
  (and the road, a control); Iolo's Bows Buy and Sell; a healer's companion list; the reagent
  seller and the guild; the Ready list with long equipped names; the Use list.

The probe is **not registered** in any CMake file.

## Running it

1. Copy `tdeck_diff_probe_test.cpp` into `native/targets/tdeck/host_tests/`, both in a baseline
   worktree (this batch used `C:\Dev\uvw-lp-base`, detached at `00ae0410`) and in this worktree.
2. The baseline tree needs the test-only fixture patch: copy this branch's
   `host_tests/a4_ui2_harness.h` and `host_tests/alpha_runtime_host_fixture.cpp` into it, and add
   `bool look_text_from_pack = false;` at the end of `AlphaRuntime::HostTestFixture` in its
   `main/alpha_runtime.h` (a field the firmware never instantiates).
3. In both trees, add after `openu5_a4_ui2_runtime(win_dos_runtime)` in `native/core/CMakeLists.txt`:

   ```cmake
   openu5_a4_ui2_runtime(tdeck_diff_probe)
   target_compile_definitions(tdeck_diff_probe PRIVATE PROBE_DOS)
   ```

4. Build `tdeck_diff_probe` in a separate build directory of each tree, run each on the same packs:

   ```sh
   tdeck_diff_probe <openu5-alpha1-resources.bin> <openu5-audio.bin> > base.txt            # and head.txt
   tdeck_diff_probe <pack> <audio> --paced > base-paced.txt                                # and head-paced.txt
   tdeck_diff_probe <pack> <audio> ABCDEFGHIJ --dos > head-dos.txt                         # positive control, head only
   ```

5. Classify (exit 1 on any unexplained walk):

   ```sh
   python lookpolish_probe_classify.py base.txt head.txt
   python lookpolish_probe_classify.py base-paced.txt head-paced.txt
   ```

6. Revert the CMake edit and delete the copied probe in both trees.
