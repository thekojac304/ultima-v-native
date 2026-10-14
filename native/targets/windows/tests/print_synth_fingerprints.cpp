// Ultima V Native for Windows -- prints the synth fingerprints of
// synth_fingerprints.h (and the two pack-free a3_04b chip goldens), so the
// values pinned in audio_backend_test.cpp can be re-derived with the host
// suite's compiler. Not part of the ctest run; built by hand, e.g. with the
// w64devkit GCC the native/core host suite uses (from the repository root):
//
//   g++ -std=c++20 -O3 -DNDEBUG -Inative/core/include -Inative/core/tests ^
//       native/targets/windows/tests/print_synth_fingerprints.cpp ^
//       native/core/src/sfx_synth.cpp native/core/src/music_synth.cpp ^
//       native/core/src/audio.cpp -o print_synth_fingerprints.exe
//
// (add any further native/core/src file the linker asks for). uv_audio_backend_test
// prints the same lines under MSVC ("--print" prints only these lines).
#include <cstdio>

#include "a3_04b_synth_goldens.h"
#include "synth_fingerprints.h"

int main() {
    using namespace openu5;
    std::printf("// per-cue SFX fingerprints: {id, frames, hash}\n");
    for (size_t i = 1; i < kSfxIdCount; ++i) {
        uint32_t frames = 0;
        const uint64_t h = uvsynth::sfx_cue_fingerprint(SfxId(i), &frames);
        if (h) std::printf("    {%zu, %u, 0x%016llxull}, // %s\n", i, frames, (unsigned long long)h, sfx_cue(SfxId(i)));
    }
    size_t supported = 0;
    const uint64_t corpus = uvsynth::sfx_corpus_fingerprint(&supported);
    std::printf("SFX_CORPUS 0x%016llx supported=%zu\n", (unsigned long long)corpus, supported);
    std::printf("SFX_PLAYER_SCRIPT 0x%016llx\n", (unsigned long long)uvsynth::sfx_player_fingerprint());
    std::printf("OPL_TABLES 0x%016llx\n", (unsigned long long)uvsynth::opl_tables_fingerprint());
    std::printf("FNUM 0x%016llx\n", (unsigned long long)uvsynth::fnum_fingerprint());
    int32_t p1 = 0, p2 = 0, p3 = 0;
    const uint64_t s1 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl2, 16000, 128, kUnityGainQ15, false, 10.0, &p1);
    const uint64_t s2 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl3, 44100, 100, volume_to_gain_q15(80), false, 10.0, &p2);
    const uint64_t s3 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl2, 22050, 96, kUnityGainQ15, true, 8.0, &p3);
    std::printf("SONG_OPL2_16K_128 0x%016llx peak=%d\n", (unsigned long long)s1, p1);
    std::printf("SONG_OPL3_44K_100_V80 0x%016llx peak=%d\n", (unsigned long long)s2, p2);
    std::printf("SONG_OPL2_22K_96_LOOP 0x%016llx peak=%d\n", (unsigned long long)s3, p3);
    std::printf("CHIP_STRESS_OPL2 0x%016llx (a3_04b golden %016llx)\n",
                (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl2, 0x5eed04b, 6000),
                (unsigned long long)a3_04b::kChipStressOpl2);
    std::printf("CHIP_STRESS_OPL3 0x%016llx (a3_04b golden %016llx)\n",
                (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl3, 0xa3a3b0b, 6000),
                (unsigned long long)a3_04b::kChipStressOpl3);
    return 0;
}
