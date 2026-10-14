# SPP2 T-Deck differential

The probe and classifier behind the T-Deck differential in
`docs/SHARED_PRESERVATION_PARITY_SPP2.md` §7.2. The probe is SPP1's
(`../spp1_differential/`), plus walk **I**. Walk I reaches what SPP2 changes on the device:

- a Mix to completion, with three endings;
- the View scroll's reveal;
- Hole up cancelled, in the world and in a town;
- a town Talk at an empty cell;
- an R cancelled beside an approaching NPC.

The probe is **not registered** in any CMake file.

## Running it

1. Copy `tdeck_diff_probe_test.cpp` into `native/targets/tdeck/host_tests/`, both in a baseline
   worktree (SPP2 used `C:\Dev\uvw-spp2-base`, detached at `fc4a63aa`) and in this worktree.
2. In both trees, add these lines after `openu5_a4_ui2_runtime(win_dos_runtime)` in
   `native/core/CMakeLists.txt`:

   ```cmake
   openu5_a4_ui2_runtime(tdeck_diff_probe)
   target_compile_definitions(tdeck_diff_probe PRIVATE PROBE_DOS)
   ```

3. Build `tdeck_diff_probe` in both trees. Then run each on the same packs:

   ```sh
   tdeck_diff_probe <openu5-alpha1-resources.bin> <openu5-audio.bin> > base.txt            # and head.txt
   tdeck_diff_probe <pack> <audio> --paced > base-paced.txt                                # and head-paced.txt
   tdeck_diff_probe <pack> <audio> ABCDEFGHI --dos > head-dos.txt                          # positive control, head only
   ```

4. Compare and classify:

   ```sh
   python ../spp1_differential/spp1_probe_compare.py base.txt head.txt --examples 1
   python spp2_probe_classify.py base.txt head.txt        # exit 1 on any unexplained walk
   python spp2_probe_classify.py base-paced.txt head-paced.txt
   ```

5. Revert the CMake edit (`git -c core.autocrlf=false checkout -- native/core/CMakeLists.txt`) and
   delete the copied probe in both trees.
