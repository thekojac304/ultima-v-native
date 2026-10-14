#pragma once
// Ultima V Native for Windows -- synth fingerprints that need no game files.
//
// Header-only and core-only (no SDL, no Windows), so the SAME definitions
// run in uv_audio_backend_test (MSVC) and in print_synth_fingerprints.cpp,
// which is built by hand with the host suite's GCC to pin the reference
// values (see that file). Everything is rendered from synthetic inputs:
//
//   sfx_cue_fingerprint      every PC-speaker cue program (SpeakerVoice) to its end
//   sfx_player_fingerprint   a scripted SfxPlayer timeline at Volume 80 %
//                            (queueing, preemption, flush/fade, rumble, ambient)
//   opl_tables_fingerprint   the OPL ROM tables music_synth.cpp builds with libm
//                            (log2/sin/pow + lround) and the exp curve
//   fnum_fingerprint         note_to_fnum_block() for notes 0..127 in quarter steps
//   synthetic_song_*         a made-up multi-channel XMI (chords, drums, program
//                            changes, pitch bend, sustain, voice stealing) through
//                            MusicSongPlayer with a made-up FAT.OPL-shaped bank
//
// Fingerprint = FNV-1a 64, byte-wise, samples little-endian (uvtest::Fnv1a64).
#include <cstdint>
#include <cstring>
#include <vector>

#include "openu5/audio.h"
#include "openu5/music_synth.h"
#include "openu5/sfx_synth.h"

namespace uvsynth {

struct Fnv {
    uint64_t h = 1469598103934665603ull;
    void byte(uint8_t b) {
        h ^= b;
        h *= 1099511628211ull;
    }
    void u16(uint16_t v) {
        byte(uint8_t(v));
        byte(uint8_t(v >> 8));
    }
    void u32(uint32_t v) {
        u16(uint16_t(v));
        u16(uint16_t(v >> 16));
    }
    void u64(uint64_t v) {
        u32(uint32_t(v));
        u32(uint32_t(v >> 32));
    }
};

// ---------------------------------------------------------------------------
// SFX
// ---------------------------------------------------------------------------
inline bool same_segment(const openu5::SpeakerSegment &a, const openu5::SpeakerSegment &b) {
    return a.kind == b.kind && a.value == b.value && a.start == b.start && a.delta == b.delta && a.iterations == b.iterations &&
           a.iteration_half_samples == b.iteration_half_samples && a.calls == b.calls && a.call_stride == b.call_stride;
}
inline bool same_program(const openu5::SpeakerProgram &a, const openu5::SpeakerProgram &b) {
    if (a.count != b.count) return false;
    for (uint8_t i = 0; i < a.count; ++i)
        if (!same_segment(a.segments[i], b.segments[i])) return false;
    return true;
}

/** One program rendered alone, unscaled, to its end (capped at 5 minutes). */
inline uint64_t voice_fingerprint(const openu5::SpeakerProgram &p, uint32_t *frames_out = nullptr) {
    using namespace openu5;
    uint16_t noise = kNoiseSeed;
    uint16_t rumble = kRumbleSeed;
    SpeakerVoice v;
    v.start(p, &noise, &rumble);
    Fnv f;
    int32_t buf[256];
    uint32_t frames = 0;
    for (size_t got; frames < 16000u * 300u && (got = v.render(buf, 256)) > 0;) {
        for (size_t i = 0; i < got; ++i) f.u32(uint32_t(buf[i]));
        frames += uint32_t(got);
    }
    f.u32(frames);
    f.u16(noise);
    f.u16(rumble);
    if (frames_out) *frames_out = frames;
    return f.h;
}

/**
 * A cue's fingerprint: its program for param 0, plus every param 1..9 whose
 * program differs from param 0's (the harpsichord digit, the ceremony index,
 * member counts...). 0 = the cue has no program (sfx_supported false).
 */
inline uint64_t sfx_cue_fingerprint(openu5::SfxId id, uint32_t *frames_out = nullptr) {
    using namespace openu5;
    SpeakerProgram base;
    if (!compile_sfx(id, 0, base)) return 0;
    Fnv f;
    f.u32(uint32_t(id));
    uint32_t total = 0, frames = 0;
    f.u64(voice_fingerprint(base, &frames));
    total += frames;
    for (int32_t param = 1; param <= 9; ++param) {
        SpeakerProgram p;
        if (!compile_sfx(id, param, p) || same_program(p, base)) continue;
        f.u32(uint32_t(param));
        f.u64(voice_fingerprint(p, &frames));
        total += frames;
    }
    if (frames_out) *frames_out = total;
    return f.h;
}

/** Every supported cue, in id order. */
inline uint64_t sfx_corpus_fingerprint(size_t *supported = nullptr) {
    using namespace openu5;
    Fnv f;
    size_t n = 0;
    for (size_t i = 1; i < kSfxIdCount; ++i) {
        const uint64_t h = sfx_cue_fingerprint(SfxId(i));
        if (!h) continue;
        ++n;
        f.u32(uint32_t(i));
        f.u64(h);
    }
    if (supported) *supported = n;
    return f.h;
}

/** One step of the scripted timeline: before block `block`, flush and/or submit. */
struct SfxScriptStep {
    uint32_t block;
    bool flush;
    openu5::SfxId id;
    int32_t param;
};

inline const SfxScriptStep *sfx_script(size_t &count) {
    using openu5::SfxId;
    static const SfxScriptStep kScript[] = {
        {0, false, SfxId::MoveBlocked, 0},
        {0, false, SfxId::MoveStep, 0},             // queued behind the bump
        {3, false, SfxId::InstrumentNote, 5},       // harpsichord keys
        {4, false, SfxId::InstrumentNote, 9},
        {5, false, SfxId::InstrumentNote, 0},
        {40, false, SfxId::CombatHit, 0},
        {41, false, SfxId::CombatDamage, 3},
        {44, false, SfxId::ApparitionMaterialize, 0}, // a scene cue preempts
        {70, true, SfxId::DungeonTrap, 0},          // flush (2 ms fade), then a new cue
        {110, false, SfxId::Quake, 0},              // the rumble
        {190, true, SfxId::None, 0},                // flush mid-rumble
        {192, false, SfxId::AmbientClockTick, 0},
        {200, false, SfxId::ShopTransaction, 0},
        {201, false, SfxId::AmbientFountain, 0},    // skipped while busy
        {230, false, SfxId::Moongate, 0},
        {260, false, SfxId::MoveBlocked, 0},        // coalescing repeats
        {260, false, SfxId::MoveBlocked, 0},
        {260, false, SfxId::MoveBlocked, 0},
        {300, false, SfxId::SpellCast, 0},          // unsupported: refused
        {301, false, SfxId::DiagnosticTone, 0},
    };
    count = sizeof kScript / sizeof kScript[0];
    return kScript;
}
constexpr uint32_t kSfxScriptBlocks = 480; // 128-frame blocks: 3.84 s at 16 kHz
constexpr uint32_t kSfxScriptBlockFrames = 128;

/** The timeline through SfxPlayer directly, at Volume 80 %. `pcm` (optional) receives every sample. */
inline uint64_t sfx_player_fingerprint(std::vector<int16_t> *pcm = nullptr) {
    using namespace openu5;
    size_t steps = 0;
    const SfxScriptStep *script = sfx_script(steps);
    SfxPlayer player;
    const uint16_t gain = volume_to_gain_q15(80);
    Fnv f;
    int16_t block[kSfxScriptBlockFrames];
    uint32_t sequence = 0;
    size_t next = 0;
    if (pcm) pcm->clear();
    for (uint32_t b = 0; b < kSfxScriptBlocks; ++b) {
        for (; next < steps && script[next].block == b; ++next) {
            if (script[next].flush) player.flush();
            if (script[next].id != SfxId::None) {
                SfxRequest r;
                r.id = script[next].id;
                r.param = script[next].param;
                r.gain_q15 = gain;
                r.sequence = ++sequence;
                player.submit(r);
            }
        }
        player.render(block, kSfxScriptBlockFrames, gain);
        for (int16_t s : block) f.u16(uint16_t(s));
        if (pcm) pcm->insert(pcm->end(), block, block + kSfxScriptBlockFrames);
    }
    return f.h;
}

// ---------------------------------------------------------------------------
// OPL tables (libm-built) and the F-number table
// ---------------------------------------------------------------------------
inline uint64_t opl_tables_fingerprint() {
    using namespace openu5;
    Fnv f;
    for (int i = 0; i < 256; ++i) f.u16(test_only::opl_log_sin(i));
    for (int i = 0; i < 256; ++i) f.u16(test_only::opl_exp(i));
    for (int32_t att = 0; att < 0x1000; ++att) f.u32(uint32_t(test_only::opl_expo(att)));
    return f.h;
}

inline uint64_t fnum_fingerprint() {
    using namespace openu5;
    Fnv f;
    for (int q = 0; q <= 127 * 4; ++q) {
        const FnumBlock fb = note_to_fnum_block(float(q) / 4.0f);
        f.u16(uint16_t(fb.fnum));
        f.u16(uint16_t(fb.block));
    }
    // Pitch-bent notes a bend wheel produces (+-2 semitones in 1/64 steps around A4).
    for (int k = -128; k <= 128; ++k) {
        const FnumBlock fb = note_to_fnum_block(69.0f + float(k) / 64.0f);
        f.u16(uint16_t(fb.fnum));
        f.u16(uint16_t(fb.block));
    }
    return f.h;
}

// ---------------------------------------------------------------------------
// A synthetic song (made-up notes and timbres; no game data)
// ---------------------------------------------------------------------------
namespace detail {
inline void be32(std::vector<uint8_t> &v, uint32_t x) {
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}
inline void fourcc(std::vector<uint8_t> &v, const char *id) { v.insert(v.end(), id, id + 4); }
inline void vlq(std::vector<uint8_t> &v, uint32_t value) {
    uint8_t stack[5];
    int n = 0;
    stack[n++] = uint8_t(value & 0x7f);
    value >>= 7;
    while (value > 0) {
        stack[n++] = uint8_t((value & 0x7f) | 0x80);
        value >>= 7;
    }
    for (int i = n - 1; i >= 0; --i) v.push_back(stack[i]);
}
inline std::vector<uint8_t> wrap_xmi(const std::vector<uint8_t> &evnt) {
    std::vector<uint8_t> form_xmid;
    fourcc(form_xmid, "EVNT");
    be32(form_xmid, uint32_t(evnt.size()));
    form_xmid.insert(form_xmid.end(), evnt.begin(), evnt.end());
    if (evnt.size() & 1) form_xmid.push_back(0);
    std::vector<uint8_t> form_chunk;
    fourcc(form_chunk, "FORM");
    be32(form_chunk, uint32_t(4 + form_xmid.size()));
    fourcc(form_chunk, "XMID");
    form_chunk.insert(form_chunk.end(), form_xmid.begin(), form_xmid.end());
    std::vector<uint8_t> cat_body;
    fourcc(cat_body, "XMID");
    cat_body.insert(cat_body.end(), form_chunk.begin(), form_chunk.end());
    std::vector<uint8_t> cat;
    fourcc(cat, "CAT ");
    be32(cat, uint32_t(cat_body.size()));
    cat.insert(cat.end(), cat_body.begin(), cat_body.end());
    std::vector<uint8_t> info;
    fourcc(info, "INFO");
    be32(info, 2);
    info.push_back(1);
    info.push_back(0);
    std::vector<uint8_t> xdir;
    fourcc(xdir, "XDIR");
    xdir.insert(xdir.end(), info.begin(), info.end());
    std::vector<uint8_t> out;
    fourcc(out, "FORM");
    be32(out, uint32_t(xdir.size()));
    out.insert(out.end(), xdir.begin(), xdir.end());
    out.insert(out.end(), cat.begin(), cat.end());
    return out;
}
struct Timbre {
    uint8_t bank, patch, fixed_note;
    uint8_t mod[5]; // 0x20, 0x40, 0x60, 0x80, 0xE0
    uint8_t fb_conn;
    uint8_t car[5];
};
} // namespace detail

inline std::vector<uint8_t> synthetic_bank() {
    using detail::Timbre;
    static const Timbre kTimbres[] = {
        {0, 0, 0, {0x21, 0x1a, 0xf2, 0x45, 0x00}, 0x0a, {0x21, 0x00, 0xf4, 0x56, 0x00}},  // soft FM, feedback 5
        {0, 1, 0, {0x31, 0x16, 0xd3, 0x24, 0x01}, 0x01, {0x22, 0x05, 0xb4, 0x35, 0x02}},  // additive, waveforms 1/2
        {0, 5, 0, {0xe1, 0x4f, 0xa1, 0x13, 0x02}, 0x0c, {0x61, 0x80, 0x82, 0x17, 0x03}},  // AM+VIB, KSL, waveform 3
        {0, 9, 0, {0x02, 0x9a, 0xf8, 0x0f, 0x00}, 0x07, {0x11, 0x40, 0xf6, 0x0c, 0x01}},  // percussive envelope
        {127, 36, 36, {0x00, 0x0b, 0xf8, 0x6b, 0x00}, 0x08, {0x00, 0x00, 0xf6, 0x4c, 0x00}}, // kick
        {127, 38, 60, {0x0c, 0x00, 0xf8, 0xb5, 0x00}, 0x0e, {0x00, 0x00, 0xd6, 0x4f, 0x00}}, // snare (noise-ish)
        {127, 42, 72, {0x01, 0x07, 0xf9, 0x9f, 0x00}, 0x0f, {0x0f, 0x00, 0xf8, 0xfd, 0x02}}, // hat
    };
    const size_t count = sizeof kTimbres / sizeof kTimbres[0];
    std::vector<uint8_t> index, blocks;
    const size_t base = count * 6 + 2;
    for (size_t i = 0; i < count; ++i) {
        const Timbre &t = kTimbres[i];
        index.push_back(t.patch);
        index.push_back(t.bank);
        const uint32_t offset = uint32_t(base + i * 14);
        for (int k = 0; k < 4; ++k) index.push_back(uint8_t(offset >> (8 * k)));
        blocks.push_back(14);
        blocks.push_back(0);
        blocks.push_back(t.fixed_note);
        blocks.insert(blocks.end(), t.mod, t.mod + 5);
        blocks.push_back(t.fb_conn);
        blocks.insert(blocks.end(), t.car, t.car + 5);
    }
    index.push_back(0xff);
    index.push_back(0xff);
    index.insert(index.end(), blocks.begin(), blocks.end());
    return index;
}

inline std::vector<uint8_t> synthetic_song_xmi() {
    using detail::vlq;
    std::vector<uint8_t> e;
    auto delay = [&](uint32_t ticks) {
        while (ticks > 0x7f) {
            e.push_back(0x7f);
            ticks -= 0x7f;
        }
        if (ticks) e.push_back(uint8_t(ticks));
    };
    auto note = [&](uint8_t ch, uint8_t key, uint8_t vel, uint32_t dur) {
        e.push_back(uint8_t(0x90 | ch));
        e.push_back(key);
        e.push_back(vel);
        vlq(e, dur);
    };
    auto cc = [&](uint8_t ch, uint8_t c, uint8_t v) {
        e.push_back(uint8_t(0xb0 | ch));
        e.push_back(c);
        e.push_back(v);
    };
    auto program = [&](uint8_t ch, uint8_t p) {
        e.push_back(uint8_t(0xc0 | ch));
        e.push_back(p);
    };
    auto bend = [&](uint8_t ch, uint16_t value14) {
        e.push_back(uint8_t(0xe0 | ch));
        e.push_back(uint8_t(value14 & 0x7f));
        e.push_back(uint8_t(value14 >> 7));
    };
    program(0, 0);
    program(1, 1);
    program(2, 5);
    program(3, 9);
    cc(0, 7, 100);
    cc(1, 7, 120);
    cc(2, 10, 20); // pan
    // bar 1: a chord, a bass note, drums
    note(0, 60, 100, 60);
    note(0, 64, 90, 60);
    note(0, 67, 80, 60);
    note(1, 36, 110, 120);
    note(9, 36, 120, 10);
    delay(30);
    note(9, 42, 90, 5);
    delay(30);
    bend(0, 0x2800); // bend up while the chord's release rings
    note(9, 38, 100, 10);
    note(2, 72, 100, 90);
    delay(30);
    cc(0, 64, 127); // sustain on
    note(9, 42, 90, 5);
    note(0, 62, 100, 15);
    delay(30);
    bend(0, 0x2000); // centre
    cc(0, 7, 64);
    note(9, 36, 120, 10);
    note(1, 43, 110, 60);
    delay(30);
    cc(0, 64, 0); // sustain off: releases the held note
    note(9, 42, 90, 5);
    delay(30);
    // bar 2: twelve notes at once on two channels -- more than OPL2's nine voices
    for (uint8_t k = 0; k < 12; ++k) note(uint8_t(k < 6 ? 0 : 3), uint8_t(48 + 3 * k), uint8_t(70 + 4 * k), uint32_t(40 + 5 * k));
    note(9, 38, 127, 10);
    delay(45);
    program(0, 5); // program change mid-song
    bend(3, 0x1000);
    for (uint8_t k = 0; k < 4; ++k) note(0, uint8_t(76 - 2 * k), 100, 20);
    delay(20);
    note(9, 36, 120, 10);
    note(2, 84, 127, 200); // a long, high note on the AM/VIB timbre
    delay(100);
    cc(2, 7, 30); // volume ramp on the held note
    delay(40);
    cc(2, 7, 0);
    delay(60);
    e.push_back(0xff); // end of track
    e.push_back(0x2f);
    vlq(e, 0);
    return detail::wrap_xmi(e);
}

/** The synthetic song through MusicSongPlayer. `seconds` caps the render. */
inline uint64_t synthetic_song_fingerprint(openu5::OplChipKind chip, uint32_t rate, size_t chunk, uint16_t gain, bool loop,
                                           double seconds, int32_t *peak = nullptr) {
    using namespace openu5;
    const std::vector<uint8_t> bank_bytes = synthetic_bank();
    MilesOplBank bank;
    if (!bank.load(bank_bytes.data(), bank_bytes.size())) return 0;
    const std::vector<uint8_t> xmi = synthetic_song_xmi();
    MusicTrack track;
    if (!parse_xmi_events(xmi.data(), xmi.size(), track)) return 0;
    MusicSongPlayer player;
    player.start(track, bank, chip, loop);
    Fnv f;
    std::vector<int16_t> buf(chunk);
    const size_t budget = size_t(seconds * double(rate));
    int32_t pk = 0;
    for (size_t rendered = 0; rendered < budget && !player.ended(); rendered += chunk) {
        player.render(buf.data(), chunk, rate, gain);
        for (int16_t v : buf) {
            f.u16(uint16_t(v));
            const int32_t a = v < 0 ? -int32_t(v) : int32_t(v);
            if (a > pk) pk = a;
        }
    }
    f.u32(uint32_t(track.events.size()));
    f.u32(track.end_tick);
    if (peak) *peak = pk;
    return f.h;
}

} // namespace uvsynth
