# SPP1 T-Deck differential

The probe and scripts behind the T-Deck differential in
`docs/SHARED_PRESERVATION_PARITY_SPP1.md`. The probe is **not registered** in any CMake file.
Register it temporarily, in both trees, to run it.

## The probe

`tdeck_diff_probe_test.cpp` drives raw T-Deck inputs through the real `AlphaRuntime` on the real
`Board`, using the a4_ui2 harness and the default `PlatformProfile`, which is the device. Inputs
are keys, trackball, Mic and Alt chords.

It covers these walks:

- every command letter, under 8 response scripts;
- the arena, including J, P and Y;
- every shop dialog 0x81..0x88, reached by Talk, including a served-then-rumour walk;
- conversations;
- towns and dungeons, letter by letter;
- the dungeon Look and Drink picks;
- Ctrl-K and Ctrl-V (raw 0x0B and 0x16; the T-Deck keyboard cannot type them);
- menus, save and load;
- the shrines.

After each input it prints:

- the hashes of the GRAM, the transcript, `GameState`, the arena, the audio calls and the last GAM;
- the mode, request and prompt;
- the turns, the clock and the RNG seed.

## Running it

1. Copy the probe into `native/targets/tdeck/host_tests/` in the baseline worktree (e089f2c5) and
   in the SPP1 worktree.
2. After `openu5_a4_ui2_runtime(win_dos_runtime)` in `native/core/CMakeLists.txt`, add these lines:

   ```cmake
   openu5_a4_ui2_runtime(tdeck_diff_probe)
   target_compile_definitions(tdeck_diff_probe PRIVATE PROBE_DOS)
   ```

3. Build `tdeck_diff_probe` in both trees, then run each on the SAME packs:

   ```sh
   tdeck_diff_probe <openu5-alpha1-resources.bin> <openu5-audio.bin> > base.txt   # and head.txt
   tdeck_diff_probe <pack> <audio> ABCDEFGH --dos > dos.txt                       # positive control, SPP1 build
   ```

4. Analyse the outputs:

   ```sh
   python spp1_probe_compare.py base.txt head.txt --examples 1   # first divergence per walk, grouped
   python spp1_probe_acheck.py base.txt head.txt                 # +1-turn divergences vs the dispatcher arm sets
   python spp1_probe_classify.py base.txt head.txt               # every differing walk -> its SPP1 item; exit 1 if any is unexplained
   ```

5. Afterwards, revert the CMake edit (`git checkout -- native/core/CMakeLists.txt`) and delete the
   copied probe in both trees.

## Limits

A walk is classified by its first differing step. Everything after that step is downstream of it,
so a second, independent change later in the same walk would be masked. The per-item host tests
(`spp1_core`, `spp1_runtime`) cover each change on its own.
