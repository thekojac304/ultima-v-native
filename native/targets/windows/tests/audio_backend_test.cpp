// Ultima V Native for Windows -- uv::SdlAudioBackend and the synth under MSVC.
//
//   O  offline (use_offline_output + render_offline): silence before a cue, a
//      supported cue renders, the backend's output is sample-for-sample the
//      core SfxPlayer's for a scripted timeline (the unchanged AudioRingPump
//      in between), stop_sfx fades to exact silence, unsupported ids and a
//      full command queue are refused
//   D  a real SDL audio device on the "dummy" driver: start() opens it, SDL's
//      audio thread pulls samples (frames_delivered grows), shutdown stops it
//   F  an audio driver that does not exist: start() fails, the backend
//      latches failed() and refuses every request
//   G  the synth's bit-exactness under MSVC: the host suite's pack-free
//      goldens (a3_04b chip stress G5/G6, the exp ROM / silent floor G7, the
//      resampler's lround rounding G8, the ROM anchors C1-C3), plus pack-free
//      fingerprints of every SFX cue, a scripted SfxPlayer timeline, the
//      libm-built OPL tables, the F-number table and a synthetic song,
//      pinned from the host suite's compiler (w64devkit GCC 16.2, -O3 and
//      -O0 agree; see print_synth_fingerprints.cpp); and, when the player's
//      own audio pack is present, the corpus goldens G1-G4 (else SKIP)
//
//   uv_audio_backend_test [openu5-audio.bin]      (or env UV_AUDIO_PACK)
//   uv_audio_backend_test --print                 only print the fingerprints
//   uv_audio_backend_test --known-bugs            run only the known-bug checks
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "a3_04b_synth_goldens.h"
#include "app/sdl_audio_backend.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"
#include "openu5/sfx_synth.h"
#include "synth_fingerprints.h"
#include "test_check.h"

using namespace openu5;
using uvtest::check;
using uvtest::format;
using uvtest::hex64;

namespace {

// ---------------------------------------------------------------------------
// Reference values from the host suite's compiler: w64devkit GCC 16.2.0,
// x86-64, built from this commit's native/core sources with -O3 -DNDEBUG
// (and -O0: identical), by print_synth_fingerprints.cpp.
// ---------------------------------------------------------------------------
struct CueGolden {
    uint32_t id;
    uint32_t frames;
    uint64_t hash;
};
constexpr CueGolden kGccCueGoldens[] = {
    {1, 2790, 0x47cfb721fb4da200ull},    {2, 2790, 0x2b4b0881fdb9ad94ull},     {3, 1488, 0xfbe156add74d2ce1ull},
    {4, 2790, 0xe79c6c057b66f512ull},    {6, 17360, 0x52c823b7fb79025full},    {8, 482112, 0xe55aa2b13f708da3ull},
    {9, 85560, 0xc96a98be1b12dc6bull},   {10, 114080, 0x25559fe3d28cd7d1ull},  {11, 20460, 0xd49d64dff2e5ed54ull},
    {12, 435550, 0x0df43fc21ece7a3bull}, {13, 8060, 0xd39fdc2057d4bd21ull},    {14, 114080, 0x9e80790a6cc28207ull},
    {15, 33480, 0x6788617f6b5a20dbull},  {16, 18600, 0xef15a371b101364eull},   {17, 14975, 0xafda4ca8b8b3f47cull},
    {18, 4464, 0xcae39ac02fcacf85ull},   {19, 37200, 0x8d860db0addecbf4ull},   {20, 24800, 0xcef615f5ae9ace1dull},
    {21, 99820, 0x76a64a72e3e34ff0ull},  {22, 4464, 0xe895e2a44145cc87ull},    {23, 595, 0xadfd2171519a8856ull},
    {25, 595, 0xd93bc94dc950f79cull},    {26, 595, 0x4ec3a6d35e16c56bull},     {27, 27, 0x20ca5ddc2ad48d45ull},
    {28, 55, 0xffbf658a65a1d614ull},     {29, 44, 0x68656a60c5197528ull},      {30, 44, 0x9f32770fc7c8bf10ull},
    {31, 1240, 0xed740365fdb6dc91ull},   {32, 343, 0xb4a48dd201b0d9b5ull},     {33, 2976, 0x7167af99a5d63703ull},
    {34, 2790, 0x744976353eb97fb4ull},   {35, 2790, 0xe36d06c2622ba995ull},    {36, 465, 0xc5fe7b8fd23da704ull},
    {37, 46, 0x502f9ae88fca5fe4ull},     {38, 744, 0xa7c1be1aa325af01ull},     {39, 744, 0x56c19105e6a1ab38ull},
    {40, 1998, 0xc8a9f9ab5ef4bb6bull},   {41, 6200, 0x24217d028b320777ull},    {42, 18600, 0x42ee1f1298c87539ull},
    {43, 3100, 0x5330b493ca9a52adull},   {44, 37200, 0x9ce60cbf890c1691ull},   {45, 14975, 0x19578f4967101038ull},
    {46, 55, 0xa56a9d071a72501aull},     {47, 88, 0x05b0a8cce4733513ull},      {48, 1116, 0xdb126680a3b78d9bull},
    {52, 55800, 0x2c4601c4dec8bc5bull},  {57, 4798, 0xb86d35f6bd93e47full},    {58, 40300, 0x02e3ae02adfd8155ull},
    {59, 18600, 0xf2ea300d5d00bd1bull},  {60, 3100, 0xd69feb8588f2c530ull},    {61, 595, 0x97b339e207ba7388ull},
    {62, 595, 0xd19bf58096868a17ull},    {63, 2790, 0x96497aeef0f1a649ull},    {64, 6510, 0x3c500a98f8620615ull},
    {65, 744, 0x4c236d69739701aeull},    {66, 1860, 0x31dfa857ea731155ull},    {67, 116066, 0x3c9d470e62facf84ull},
    {68, 744, 0x3e967bef2010b95bull},    {69, 2790, 0xc022e94e6fc70abcull},    {70, 4572880, 0xf998569cd167b01dull},
    {71, 744, 0x2c1cddf7e0a8bb58ull},    {72, 161200, 0x6f7453f2c3a8c66cull},  {73, 725400, 0x9f0154b197cbad38ull},
};
constexpr size_t kGccSupportedCues = 63;
constexpr uint64_t kGccSfxCorpus = 0x8bee717a062c3187ull;
constexpr uint64_t kGccSfxPlayerScript = 0x1beb56164d8996c6ull;
constexpr uint64_t kGccOplTables = 0x147440df934242b9ull;
constexpr uint64_t kGccFnum = 0x8c580b1fbc2288adull;
constexpr uint64_t kGccSongOpl2_16k = 0xac329427866f714bull;
constexpr uint64_t kGccSongOpl3_44k_v80 = 0x42f7af7bc8370d56ull;
constexpr uint64_t kGccSongOpl2_22k_loop = 0x937af929760c5b50ull;

SfxRequest request(SfxId id, int32_t param = 0, uint16_t gain = kUnityGainQ15) {
    SfxRequest r;
    r.id = id;
    r.param = param;
    r.gain_q15 = gain;
    return r;
}

size_t nonzero(const std::vector<int16_t> &v, size_t from = 0, size_t to = SIZE_MAX) {
    size_t n = 0;
    for (size_t i = from; i < v.size() && i < to; ++i) n += v[i] != 0;
    return n;
}

int peak(const std::vector<int16_t> &v, size_t from = 0, size_t to = SIZE_MAX) {
    int m = 0;
    for (size_t i = from; i < v.size() && i < to; ++i) m = std::max(m, std::abs(int(v[i])));
    return m;
}

std::vector<int16_t> render(uv::SdlAudioBackend &b, size_t frames) {
    std::vector<int16_t> out(frames, int16_t(0x5555)); // a pattern: untouched samples would show
    b.render_offline(out.data(), frames);
    return out;
}

/** The scripted timeline (synth_fingerprints.h) through the backend, block by block. */
std::vector<int16_t> backend_script_pcm() {
    uv::SdlAudioBackend b;
    b.use_offline_output();
    const uint16_t gain = volume_to_gain_q15(80);
    b.set_gain(AudioChannel::Sfx, gain);
    b.set_gain(AudioChannel::Music, gain);
    size_t steps = 0;
    const uvsynth::SfxScriptStep *script = uvsynth::sfx_script(steps);
    std::vector<int16_t> pcm;
    std::vector<int16_t> block(uvsynth::kSfxScriptBlockFrames);
    size_t next = 0;
    uint32_t sequence = 0;
    for (uint32_t blk = 0; blk < uvsynth::kSfxScriptBlocks; ++blk) {
        for (; next < steps && script[next].block == blk; ++next) {
            if (script[next].flush) b.stop_sfx();
            if (script[next].id != SfxId::None) {
                SfxRequest r = request(script[next].id, script[next].param, gain);
                r.sequence = ++sequence;
                b.play_sfx(r); // SpellCast is refused here; the player refuses it too
            }
        }
        b.render_offline(block.data(), block.size());
        pcm.insert(pcm.end(), block.begin(), block.end());
    }
    return pcm;
}

uint64_t fingerprint(const std::vector<int16_t> &pcm) {
    uvsynth::Fnv f;
    for (int16_t s : pcm) f.u16(uint16_t(s));
    return f.h;
}

// ---------------------------------------------------------------------------
// O -- offline
// ---------------------------------------------------------------------------
void offline() {
    {
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.set_gain(AudioChannel::Sfx, kUnityGainQ15);
        const auto quiet = render(b, 1024);
        check(nonzero(quiet) == 0, "O1 before any cue the offline output is exact silence (1024 frames)",
              format("%zu non-zero", nonzero(quiet)));
        check(b.play_sfx(request(SfxId::MoveBlocked)), "O2 play_sfx(MoveBlocked) is accepted (a supported cue)");
        const auto cue = render(b, 4096);
        check(nonzero(cue) > 1000 && peak(cue) > 1000, "O3 render_offline after the cue is non-silent",
              format("%zu non-zero, peak %d", nonzero(cue), peak(cue)));
        // move-blocked is beep(0xa5, 0xc8): 2976 frames, then silence again.
        check(nonzero(cue, 3072) == 0, "O4 ... and returns to exact silence after the cue's 2976 frames",
              format("%zu non-zero after frame 3072", nonzero(cue, 3072)));
        check(!b.running() && !b.failed(), "O5 offline mode opens no device and does not fail");
        AudioPerfSnapshot snap{};
        check(b.perf_snapshot(snap) && snap.blocks > 0 && snap.sfx_submitted >= 1,
              "O6 the pump's perf counters were published (blocks rendered, one cue submitted)",
              format("blocks=%u sfx=%u", snap.blocks, snap.sfx_submitted));
    }
    {
        // Equivalence: the backend (AudioRingPump in between) == SfxPlayer directly.
        std::vector<int16_t> direct;
        const uint64_t direct_h = uvsynth::sfx_player_fingerprint(&direct);
        const std::vector<int16_t> via_backend = backend_script_pcm();
        size_t first_diff = SIZE_MAX;
        for (size_t i = 0; i < direct.size() && i < via_backend.size(); ++i)
            if (direct[i] != via_backend[i]) {
                first_diff = i;
                break;
            }
        check(via_backend.size() == direct.size() && first_diff == SIZE_MAX,
              format("O7 the backend's offline output equals the core SfxPlayer's, sample for sample, over a %u-block "
                     "scripted timeline (queue, preempt, flush, rumble, ambient, refusals)",
                     uvsynth::kSfxScriptBlocks),
              format("sizes %zu/%zu, first difference at sample %zu", via_backend.size(), direct.size(), first_diff));
        check(nonzero(via_backend) > 10000, "O8 the scripted timeline is mostly sound (not a trivially silent match)",
              format("%zu non-zero samples", nonzero(via_backend)));
        uvtest::note("backend script fingerprint %s, SfxPlayer %s", hex64(fingerprint(via_backend)).c_str(), hex64(direct_h).c_str());
    }
    {
        // stop_sfx: the playing cue fades over 2 ms (32 frames) and the output is silence after it.
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.set_gain(AudioChannel::Sfx, kUnityGainQ15);
        b.play_sfx(request(SfxId::InstrumentNote, 5)); // 2480 frames
        const auto before = render(b, 256);
        b.stop_sfx();
        const auto after = render(b, 1024);
        const auto later = render(b, 2048);
        check(nonzero(before) > 100, "O9 a harpsichord note sounds before stop_sfx");
        check(nonzero(after, SfxPlayer::kReleaseFrames) == 0 && nonzero(later) == 0,
              format("O10 after stop_sfx the output is exact silence from the end of the %u-frame fade on",
                     SfxPlayer::kReleaseFrames),
              format("%zu non-zero after the fade, %zu later", nonzero(after, SfxPlayer::kReleaseFrames), nonzero(later)));
        check(peak(after, 0, SfxPlayer::kReleaseFrames) <= peak(before), "O11 the fade never exceeds the note's level");
        check(b.play_sfx(request(SfxId::MoveBlocked)) && nonzero(render(b, 1024)) > 100, "O12 a new cue plays after the flush");
    }
    {
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.set_gain(AudioChannel::Sfx, kUnityGainQ15);
        const SfxId refused[] = {SfxId::None, SfxId::SpellCast, SfxId::PotionUsed, SfxId::ScrollUsed, SfxId::InvalidMagic,
                                 SfxId::CastSpell, SfxId::Count};
        bool all = true;
        for (SfxId id : refused) all = all && !sfx_supported(id) && !b.play_sfx(request(id));
        check(all, "O13 ids with no synth program (None, SpellCast, PotionUsed, ScrollUsed, InvalidMagic, CastSpell, Count) are refused");
        check(nonzero(render(b, 1024)) == 0, "O14 ... and nothing plays");
    }
    {
        // The game thread never waits: a full command queue refuses (16 slots).
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.set_gain(AudioChannel::Sfx, kUnityGainQ15);
        int accepted = 0;
        for (int i = 0; i < 17; ++i) accepted += b.play_sfx(request(SfxId::InstrumentNote, i % 10)) ? 1 : 0;
        check(accepted == 16 && b.sfx_queue_full() == 1, "O15 the 17th undrained request is refused and counted (queue of 16)",
              format("accepted=%d full=%llu", accepted, (unsigned long long)b.sfx_queue_full()));
        render(b, 128);
        check(b.play_sfx(request(SfxId::MoveStep)), "O16 after one block the queue is drained and accepts again");
    }
    {
        // No library: music requests are taken but nothing sounds.
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.set_music_library(nullptr);
        b.set_gain(AudioChannel::Music, kUnityGainQ15);
        check(b.start_music(MusicSong::Theme, kUnityGainQ15) && nonzero(render(b, 2048)) == 0 && b.music_length_ms(MusicSong::Theme) == 0,
              "O17 without the music pack start_music is accepted, renders silence, and the song has no length");
        b.stop_music();
        check(nonzero(render(b, 256)) == 0, "O18 stop_music without a song is harmless");
    }
    {
        // Gain 0 (the backend's state before the service attaches): exact silence even with a cue.
        uv::SdlAudioBackend b;
        b.use_offline_output();
        b.play_sfx(request(SfxId::MoveBlocked));
        check(nonzero(render(b, 2048)) == 0, "O19 with SFX gain 0 a cue renders exact silence (0 % stops the synth)");
    }
}

// ---------------------------------------------------------------------------
// D -- a real device on SDL's dummy driver;  F -- a driver that does not exist
// ---------------------------------------------------------------------------
void device() {
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    const bool init = SDL_InitSubSystem(SDL_INIT_AUDIO);
    check(init, "D1 SDL_InitSubSystem(SDL_INIT_AUDIO) with SDL_AUDIO_DRIVER=dummy", SDL_GetError());
    if (!init) return;
    const char *driver = SDL_GetCurrentAudioDriver();
    check(driver && std::strcmp(driver, "dummy") == 0, "D2 the current audio driver is \"dummy\"", driver ? driver : "(null)");
    {
        uv::SdlAudioBackend b;
        b.set_gain(AudioChannel::Sfx, kUnityGainQ15);
        const bool started = b.start();
        check(started && b.running() && !b.failed(), "D3 start() opens the default playback stream", b.error());
        const uint64_t f0 = b.frames_delivered();
        SDL_Delay(300);
        const uint64_t f1 = b.frames_delivered();
        SDL_Delay(300);
        const uint64_t f2 = b.frames_delivered();
        check(f1 > f0 && f2 > f1, "D4 frames_delivered() grows while SDL's audio thread pulls (two 300 ms windows)",
              format("%llu -> %llu -> %llu", (unsigned long long)f0, (unsigned long long)f1, (unsigned long long)f2));
        uvtest::note("dummy device: %llu frames in ~600 ms (%.0f frames/s at the 16 kHz stream input)",
                     (unsigned long long)(f2 - f0), double(f2 - f0) / 0.6);
        check(f2 - f0 > 2000 && f2 - f0 < 200000, "D5 the pull rate is plausible for a 16 kHz stream (2k..200k frames in 600 ms)");
        check(b.play_sfx(request(SfxId::MoveBlocked)), "D6 play_sfx is accepted while the device runs");
        SDL_Delay(300);
        AudioPerfSnapshot snap{};
        check(b.perf_snapshot(snap) && snap.sfx_submitted >= 1 && snap.blocks > 0,
              "D7 the audio thread drained the cue (perf snapshot: sfx_submitted >= 1)",
              format("sfx=%u blocks=%u", snap.sfx_submitted, snap.blocks));
        b.shutdown();
        const uint64_t g0 = b.frames_delivered();
        SDL_Delay(150);
        check(!b.running() && b.frames_delivered() == g0, "D8 shutdown() closes the stream; no more frames are pulled");
        b.shutdown();
        check(!b.running(), "D9 a second shutdown() is harmless");
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "uv-no-such-driver", SDL_HINT_OVERRIDE);
    {
        uv::SdlAudioBackend b;
        const bool started = b.start();
        check(!started && b.failed() && !b.running(), "F1 with an audio driver that does not exist, start() returns false and failed() is true");
        check(!b.error().empty(), "F2 the failure carries SDL's reason", b.error());
        uvtest::note("invalid-driver error: %s", b.error().c_str());
        check(!b.play_sfx(request(SfxId::MoveBlocked)), "F3 play_sfx returns false once failed");
        check(!b.start_music(MusicSong::Theme, kUnityGainQ15), "F4 start_music returns false once failed");
        b.stop_music();
        b.stop_sfx();
        check(!b.start() && b.failed(), "F5 the failure is latched (start() stays false)");
    }
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// G -- the synth under MSVC
// ---------------------------------------------------------------------------
void goldens_pack_free() {
    // a3_04b G5/G6: the chip under random register writes (raw float bits hashed).
    const uint64_t s2 = a3_04b::chip_stress(OplChipKind::Opl2, 0x5eed04b, 6000);
    check(s2 == a3_04b::kChipStressOpl2, "G5 a3_04b OPL2 chip stress fingerprint matches the host golden",
          hex64(s2) + " vs " + hex64(a3_04b::kChipStressOpl2));
    const uint64_t s3 = a3_04b::chip_stress(OplChipKind::Opl3, 0xa3a3b0b, 6000);
    check(s3 == a3_04b::kChipStressOpl3, "G6 a3_04b OPL3 chip stress fingerprint matches the host golden",
          hex64(s3) + " vs " + hex64(a3_04b::kChipStressOpl3));

    // a3_04b G7: the silent floor is a property of the exp ROM.
    int rom_max = 0;
    for (int i = 0; i < 256; ++i) rom_max = std::max(rom_max, int(test_only::opl_exp(i)));
    bool zero_above = true;
    for (int32_t att = OplEmulator::kSilentFloor; att <= 0x2400; ++att) zero_above = zero_above && test_only::opl_expo(att) == 0;
    check(rom_max + 2048 < 4096 && zero_above && test_only::opl_expo(OplEmulator::kSilentFloor - 1) > 0,
          "G7 kSilentFloor = 3072 is exact (exp ROM max + 2048 < 4096; expo() is 0 from 3072 up)", format("rom max %d", rom_max));

    // a3_04_music_synth C1-C3: the ROM anchors.
    check(test_only::opl_log_sin(0) == 2137 && test_only::opl_log_sin(255) == 0 && test_only::opl_exp(0) == 0 &&
              test_only::opl_expo(0) == 4085,
          "C1-C3 ROM anchors: logsin[0]=2137, logsin[255]=0, exp[0]=0, expo(0)=4085 (libm-built tables)",
          format("%u %u %u %d", test_only::opl_log_sin(0), test_only::opl_log_sin(255), test_only::opl_exp(0),
                 test_only::opl_expo(0)));

    // a3_04b G8: the resampler's integer rounding is std::lround, value for value (dense sample).
    uint64_t tested = 0, wrong = 0;
    auto test_round = [&](float v) {
        ++tested;
        const long want = std::lround(double(v) * 32767.0);
        if (long(test_only::opl_round_q15(v)) != want) ++wrong;
    };
    uint32_t x = 12345;
    for (int i = 0; i < 4000000; ++i) {
        x = x * 1664525u + 1013904223u;
        float v;
        const uint32_t b32 = (x & 0x80000000u) | (x % 0x3f800001u);
        std::memcpy(&v, &b32, sizeof v);
        test_round(v);
    }
    for (int k = -32768; k <= 32767; ++k) {
        const float v = float((double(k) + 0.5) / 32767.0);
        test_round(v);
        test_round(std::nextafter(v, 2.0f));
        test_round(std::nextafter(v, -2.0f));
    }
    check(wrong == 0, format("G8 round_q15 == std::lround(double(v) * 32767.0) for %llu floats (MSVC's lround)", (unsigned long long)tested),
          format("%llu differ", (unsigned long long)wrong));

    // Per-cue SFX fingerprints against the GCC reference.
    size_t supported = 0, matched = 0;
    std::string mismatches;
    for (size_t i = 1; i < kSfxIdCount; ++i) {
        uint32_t frames = 0;
        const uint64_t h = uvsynth::sfx_cue_fingerprint(SfxId(i), &frames);
        const CueGolden *g = nullptr;
        for (const CueGolden &c : kGccCueGoldens)
            if (c.id == i) g = &c;
        if (!h && !g) continue;
        if (h) ++supported;
        const bool ok = g && h == g->hash && frames == g->frames;
        if (ok) ++matched;
        check(ok, format("SFX%02zu cue \"%s\" renders bit-exact to the GCC reference (%u frames)", i, sfx_cue(SfxId(i)),
                         g ? g->frames : 0u),
              g ? format("MSVC %s/%u frames vs GCC %s/%u frames", hex64(h).c_str(), frames, hex64(g->hash).c_str(), g->frames)
                : "a cue the GCC reference does not support");
        if (!ok) mismatches += format(" %zu", i);
    }
    check(supported == kGccSupportedCues && matched == kGccSupportedCues,
          format("SFX the %zu supported cues all match (%zu matched)", kGccSupportedCues, matched), "mismatched ids:" + mismatches);
    const uint64_t corpus = uvsynth::sfx_corpus_fingerprint();
    check(corpus == kGccSfxCorpus, "SFX corpus fingerprint == GCC reference", hex64(corpus) + " vs " + hex64(kGccSfxCorpus));
    const uint64_t script = uvsynth::sfx_player_fingerprint();
    check(script == kGccSfxPlayerScript, "SFX scripted SfxPlayer timeline (Volume 80 %) == GCC reference",
          hex64(script) + " vs " + hex64(kGccSfxPlayerScript));
    const uint64_t backend = fingerprint(backend_script_pcm());
    check(backend == kGccSfxPlayerScript, "SFX the same timeline through SdlAudioBackend (offline) == GCC reference",
          hex64(backend) + " vs " + hex64(kGccSfxPlayerScript));

    // Music: libm-built tables, F-numbers, a synthetic song.
    const uint64_t tables = uvsynth::opl_tables_fingerprint();
    check(tables == kGccOplTables, "MUS OPL log-sin / exp / expo tables (std::log2, std::sin, std::pow, std::lround) == GCC reference",
          hex64(tables) + " vs " + hex64(kGccOplTables));
    const uint64_t fnum = uvsynth::fnum_fingerprint();
    check(fnum == kGccFnum, "MUS note_to_fnum_block (std::pow, float) for 509 notes + 257 bends == GCC reference",
          hex64(fnum) + " vs " + hex64(kGccFnum));
    int32_t p1 = 0, p2 = 0, p3 = 0;
    const uint64_t s16 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl2, 16000, 128, kUnityGainQ15, false, 10.0, &p1);
    check(s16 == kGccSongOpl2_16k && p1 > 1000, "MUS synthetic song, OPL2, 16 kHz, 128-frame blocks (the device geometry) == GCC reference",
          hex64(s16) + " vs " + hex64(kGccSongOpl2_16k) + format(" peak %d", p1));
    const uint64_t s44 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl3, 44100, 100, volume_to_gain_q15(80), false, 10.0, &p2);
    check(s44 == kGccSongOpl3_44k_v80 && p2 > 1000, "MUS synthetic song, OPL3, 44.1 kHz, 100-frame blocks, Music 80 % == GCC reference",
          hex64(s44) + " vs " + hex64(kGccSongOpl3_44k_v80) + format(" peak %d", p2));
    const uint64_t s22 = uvsynth::synthetic_song_fingerprint(OplChipKind::Opl2, 22050, 96, kUnityGainQ15, true, 8.0, &p3);
    check(s22 == kGccSongOpl2_22k_loop && p3 > 1000, "MUS synthetic song, OPL2, 22.05 kHz, 96-frame blocks, looping 8 s == GCC reference",
          hex64(s22) + " vs " + hex64(kGccSongOpl2_22k_loop) + format(" peak %d", p3));
}

/** The argument's pack if that file exists, else UV_AUDIO_PACK, else the argument (to name it in the SKIP). */
std::string audio_pack_path(int argc, char **argv) {
    std::string arg;
    for (int i = 1; i < argc; ++i)
        if (argv[i][0] != '-') arg = argv[i];
    if (!arg.empty() && std::ifstream(arg, std::ios::binary)) return arg;
    const char *env = std::getenv("UV_AUDIO_PACK");
    if (env && *env) return env;
    return arg;
}

void goldens_with_pack(const std::string &path) {
    if (path.empty()) {
        uvtest::skip("G1-G4", "no audio pack path given (argument or UV_AUDIO_PACK)");
        return;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        uvtest::skip("G1-G4 corpus goldens", "the player's audio pack is not present (" + path +
                                                 "; made by `npm run pack:audio` from their own game files)");
        return;
    }
    const std::vector<uint8_t> pack((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    AudioPackPayload payload{};
    const AudioPackInfo info = inspect_audio_pack(pack.data(), pack.size(), &payload);
    if (info.state != AudioPackState::Valid || info.record.capability != MusicCapability::SupportedMusicPatch) {
        uvtest::skip("G1-G4 corpus goldens", format("%s is not a Valid pack with the supported music patch (state %s)", path.c_str(),
                                                    audio_pack_state_name(info.state)));
        return;
    }
    check(true, "G0 the audio pack is present, Valid and carries the supported music patch");
    MilesOplBank bank;
    check(bank.load(payload.bank, payload.bank_length), "G0 its FAT.OPL bank loads");
    int32_t music_peak = 0;
    const uint64_t l6 = a3_04b::corpus_l6(payload, bank, &music_peak);
    check(l6 == a3_04b::kCorpusL6, "G1 A3-04's L6 corpus fingerprint (every song once, 256-frame chunks, 16 kHz, unity)",
          hex64(l6) + " vs " + hex64(a3_04b::kCorpusL6));
    const uint64_t dev = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 128, 16000, 30.0);
    check(dev == a3_04b::kCorpusDevice128, "G2 the device geometry: every song looping 30 s, 128-frame blocks, 16 kHz, Music 80 %",
          hex64(dev) + " vs " + hex64(a3_04b::kCorpusDevice128));
    const uint64_t opl3 = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl3, 128, 16000, 10.0);
    check(opl3 == a3_04b::kCorpusOpl3, "G3 the corpus through the OPL3 chip", hex64(opl3) + " vs " + hex64(a3_04b::kCorpusOpl3));
    const uint64_t rates = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 44100, 4.0) ^
                           (a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 22050, 4.0) * 3) ^
                           (a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 96, 16000, 4.0) * 7);
    check(rates == a3_04b::kCorpusRates, "G4 other output rates and chunk sizes (44.1 kHz, 22.05 kHz, 96 frames)",
          hex64(rates) + " vs " + hex64(a3_04b::kCorpusRates));
    uvtest::note("corpus peak at unity gain: %d", music_peak);

    // The backend plays a real song from the pack (offline).
    uv::SdlAudioBackend b;
    b.use_offline_output();
    b.set_music_library(&payload);
    b.set_gain(AudioChannel::Music, volume_to_gain_q15(80));
    check(b.music_length_ms(MusicSong::Theme) > 10000, "G9 the backend's library knows the Theme's length",
          format("%u ms", b.music_length_ms(MusicSong::Theme)));
    check(b.start_music(MusicSong::Theme, volume_to_gain_q15(80)), "G10 start_music(Theme) is accepted");
    const auto pcm = render(b, 16000 * 2);
    check(nonzero(pcm) > 8000 && peak(pcm) > 200, "G11 two seconds of the Theme are audible through the backend",
          format("%zu non-zero, peak %d", nonzero(pcm), peak(pcm)));
}

void print_fingerprints() {
    std::printf("// per-cue SFX fingerprints: {id, frames, hash}\n"); // same text as print_synth_fingerprints.cpp
    for (size_t i = 1; i < kSfxIdCount; ++i) {
        uint32_t frames = 0;
        const uint64_t h = uvsynth::sfx_cue_fingerprint(SfxId(i), &frames);
        if (h) std::printf("    {%zu, %u, 0x%016llxull}, // %s\n", i, frames, (unsigned long long)h, sfx_cue(SfxId(i)));
    }
    size_t supported = 0;
    const uint64_t corpus = uvsynth::sfx_corpus_fingerprint(&supported); // before printf: argument order is unspecified
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
                (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl2, 0x5eed04b, 6000), (unsigned long long)a3_04b::kChipStressOpl2);
    std::printf("CHIP_STRESS_OPL3 0x%016llx (a3_04b golden %016llx)\n",
                (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl3, 0xa3a3b0b, 6000), (unsigned long long)a3_04b::kChipStressOpl3);
}

// ---------------------------------------------------------------------------
// Known bugs (run with --known-bugs; registered disabled, see CMakeLists.txt)
// ---------------------------------------------------------------------------
void known_bugs() {
    // KNOWN-BUG (test helper only): render_offline() promises "exactly what the
    // SDL callback would hand the device for `frames` samples", but when
    // `frames` is not a multiple of the 128-frame block, StreamSink::deliver
    // copies only the part of the last block that fits and DROPS the rest
    // (sdl_audio_backend.cpp, StreamSink::deliver / render_offline). The SDL
    // path keeps that remainder in the stream for the next pull, so two
    // offline calls of 100 + 156 frames differ from one call of 256 frames.
    uv::SdlAudioBackend whole;
    whole.use_offline_output();
    whole.set_gain(AudioChannel::Sfx, kUnityGainQ15);
    whole.play_sfx(request(SfxId::InstrumentNote, 5));
    const auto one = render(whole, 256);
    uv::SdlAudioBackend split;
    split.use_offline_output();
    split.set_gain(AudioChannel::Sfx, kUnityGainQ15);
    split.play_sfx(request(SfxId::InstrumentNote, 5));
    auto two = render(split, 100);
    const auto rest = render(split, 156);
    two.insert(two.end(), rest.begin(), rest.end());
    size_t diff = SIZE_MAX;
    for (size_t i = 0; i < one.size(); ++i)
        if (one[i] != two[i]) {
            diff = i;
            break;
        }
    check(diff == SIZE_MAX, "K1 render_offline(100) + render_offline(156) == render_offline(256) (stream continuity)",
          format("first difference at sample %zu: the 28 frames left in the first call's block were dropped", diff));
}

} // namespace

int main(int argc, char **argv) {
    bool print = false, bugs = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--print") == 0) print = true;
        if (std::strcmp(argv[i], "--known-bugs") == 0) bugs = true;
    }
    if (print) {
        print_fingerprints();
        return 0;
    }
    if (bugs) {
        std::printf("uv_audio_backend_test --known-bugs: checks that document open defects (expected to FAIL until fixed)\n");
        known_bugs();
        return uvtest::finish("uv_audio_backend_test --known-bugs");
    }
    std::printf("uv_audio_backend_test: uv::SdlAudioBackend (offline, dummy device, failed device) and MSVC synth goldens\n");
    offline();
    device();
    goldens_pack_free();
    goldens_with_pack(audio_pack_path(argc, argv));
    return uvtest::finish("uv_audio_backend_test");
}
