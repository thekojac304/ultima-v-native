// Ultima V Native for Windows -- the on-card save layout under MSVC.
//
// 1. tdeck::AlphaSaveCommit (native/targets/tdeck/main/alpha_save_generation.h)
//    is written and read as RAW BYTES: alpha_save.cpp writes
//    write_file(path, &next, sizeof(next)) and read_commit() accepts a file
//    only if its size is exactly sizeof(Commit), then memcpy's it. A card
//    written by the T-Deck (ESP32-S3, Xtensa LX7: little-endian, uint64_t
//    aligned to 8 by the Xtensa ABI, GCC) must therefore read back on Windows
//    with the SAME size and field offsets:
//
//        offset  size  field      device value
//        0       4     magic      0x31533555 ("U5S1" in file order)
//        4       4     version    1
//        8       8     sequence   card-wide save counter (8-aligned)
//        16      4     gam        CRC-32 of the .gam file
//        20      4     ool        CRC-32 of the .ool file
//        24      4     json       CRC-32 of the .json sidecar
//        28      4     (tail padding to the 8-byte alignment of sequence)
//        size 32, alignof 8
//
//    MSVC x64 also aligns uint64_t to 8, so the layout is identical; the
//    static_asserts make any drift a compile error, and the runtime checks
//    read a byte image exactly as the device lays it out.
//
// 2. settings.json (openu5::encode_settings / decode_settings): a T-Deck card's
//    settings must load on Windows and the other way round.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "alpha_save_generation.h"
#include "openu5/frontend.h"
#include "openu5/frontend_settings.h"
#include "openu5/persistence.h"
#include "test_check.h"

using tdeck::AlphaSaveCommit;
using uvtest::check;
using uvtest::format;

static_assert(sizeof(AlphaSaveCommit) == 32, "AlphaSaveCommit must be the 32-byte record the T-Deck writes");
static_assert(alignof(AlphaSaveCommit) == 8, "AlphaSaveCommit is 8-aligned (its uint64_t sequence)");
static_assert(std::is_standard_layout_v<AlphaSaveCommit>, "offsetof needs a standard-layout record");
static_assert(std::is_trivially_copyable_v<AlphaSaveCommit>, "the record is memcpy'd to and from the card");
static_assert(offsetof(AlphaSaveCommit, magic) == 0, "magic at 0");
static_assert(offsetof(AlphaSaveCommit, version) == 4, "version at 4");
static_assert(offsetof(AlphaSaveCommit, sequence) == 8, "sequence at 8");
static_assert(offsetof(AlphaSaveCommit, gam) == 16, "gam CRC at 16");
static_assert(offsetof(AlphaSaveCommit, ool) == 20, "ool CRC at 20");
static_assert(offsetof(AlphaSaveCommit, json) == 24, "json CRC at 24");
static_assert(sizeof(AlphaSaveCommit::sequence) == 8 && sizeof(AlphaSaveCommit::magic) == 4 && sizeof(AlphaSaveCommit::json) == 4,
              "field widths");

namespace {

void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
void put64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = uint8_t(v >> (8 * i));
}

void commit_record() {
    check(sizeof(AlphaSaveCommit) == 32, "C1 sizeof(AlphaSaveCommit) == 32 (static_assert too)");
    check(offsetof(AlphaSaveCommit, magic) == 0 && offsetof(AlphaSaveCommit, version) == 4 &&
              offsetof(AlphaSaveCommit, sequence) == 8 && offsetof(AlphaSaveCommit, gam) == 16 &&
              offsetof(AlphaSaveCommit, ool) == 20 && offsetof(AlphaSaveCommit, json) == 24,
          "C2 field offsets 0/4/8/16/20/24 (magic, version, sequence, gam, ool, json)");

    const AlphaSaveCommit d{};
    check(d.magic == 0x31533555u && d.version == 1 && d.sequence == 0 && d.gam == 0 && d.ool == 0 && d.json == 0,
          "C3 a default record is magic 0x31533555, version 1, everything else 0");

    // The bytes a Windows save writes (alpha_save.cpp: write_file(..., &next, sizeof(next))).
    AlphaSaveCommit c{};
    c.sequence = 0x0102030405060708ull;
    c.gam = 0xA1A2A3A4u;
    c.ool = 0xB1B2B3B4u;
    c.json = 0xC1C2C3C4u;
    uint8_t bytes[sizeof(AlphaSaveCommit)];
    std::memcpy(bytes, &c, sizeof c);
    check(std::memcmp(bytes, "U5S1", 4) == 0, "C4 the file starts with the bytes \"U5S1\" (magic, little-endian)");
    const uint8_t version_le[4] = {1, 0, 0, 0};
    const uint8_t sequence_le[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    const uint8_t gam_le[4] = {0xA4, 0xA3, 0xA2, 0xA1};
    const uint8_t ool_le[4] = {0xB4, 0xB3, 0xB2, 0xB1};
    const uint8_t json_le[4] = {0xC4, 0xC3, 0xC2, 0xC1};
    check(std::memcmp(bytes + 4, version_le, 4) == 0 && std::memcmp(bytes + 8, sequence_le, 8) == 0 &&
              std::memcmp(bytes + 16, gam_le, 4) == 0 && std::memcmp(bytes + 20, ool_le, 4) == 0 &&
              std::memcmp(bytes + 24, json_le, 4) == 0,
          "C5 version/sequence/CRCs are little-endian at 4/8/16/20/24, as the ESP32-S3 stores them");

    // A record as the T-Deck lays it out, built byte by byte, read the way read_commit() does.
    uint8_t card[32];
    std::memset(card, 0xEE, sizeof card); // the tail padding is whatever the device left there
    put32(card + 0, 0x31533555u);
    put32(card + 4, 1);
    put64(card + 8, 4242424242ull);
    put32(card + 16, 0x11223344u);
    put32(card + 20, 0x55667788u);
    put32(card + 24, 0x99AABBCCu);
    AlphaSaveCommit r;
    std::memcpy(&r, card, sizeof r);
    check(r.magic == 0x31533555u && r.version == 1 && r.sequence == 4242424242ull && r.gam == 0x11223344u &&
              r.ool == 0x55667788u && r.json == 0x99AABBCCu,
          "C6 a 32-byte device image reads back field for field (padding bytes ignored)");

    // The save target rule works on records read that way.
    AlphaSaveCommit pair[2]{};
    pair[0].sequence = 7;
    pair[1].sequence = 9;
    const bool present[2] = {true, true};
    check(tdeck::newest_committed_slot(present, pair) == 1, "C7 newest_committed_slot picks the higher sequence");
    const tdeck::AlphaSaveTarget t = tdeck::choose_save_target(present, pair, true, 9);
    check(t.slot == 0 && t.keep == 1 && t.sequence == 10, "C8 choose_save_target rotates to the other slot with sequence + 1",
          format("slot=%d keep=%d sequence=%llu", t.slot, t.keep, (unsigned long long)t.sequence));

    // files_match_commit: CRC-32 of each file against the record.
    tdeck::AlphaSaveCandidate cand;
    cand.gam = {1, 2, 3, 4, 5};
    cand.ool = {9, 8, 7};
    cand.json = {'{', '}'};
    cand.commit.gam = openu5::save::save_crc32(cand.gam.data(), cand.gam.size());
    cand.commit.ool = openu5::save::save_crc32(cand.ool.data(), cand.ool.size());
    cand.commit.json = openu5::save::save_crc32(cand.json.data(), cand.json.size());
    const bool match = tdeck::files_match_commit(cand);
    cand.json.push_back(' ');
    const bool mismatch = tdeck::files_match_commit(cand);
    check(match && !mismatch, "C9 files_match_commit accepts the recorded CRC-32s and refuses a changed sidecar");
    const uint8_t check_bytes[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(openu5::save::save_crc32(check_bytes, sizeof check_bytes) == 0xCBF43926u,
          "C10 save_crc32 is the IEEE CRC-32 (\"123456789\" -> 0xCBF43926), the packer's and the device's");
}

std::string describe(const openu5::FrontendSettings &s) {
    return format("{v=%u bright=%u move=%d tb=%u speed=%u ui=%u dev=%d sfx=%u music=%u touch=%d light=%u}", unsigned(s.version),
                  unsigned(s.brightness), int(s.movement_mode), unsigned(s.trackball_responsiveness),
                  unsigned(s.trackball_speed), unsigned(s.ui_size), int(s.developer_tools_visible), unsigned(s.sound_volume),
                  unsigned(s.music_volume), int(s.touch_controls), unsigned(s.keyboard_backlight));
}

bool same(const openu5::FrontendSettings &a, const openu5::FrontendSettings &b) {
    return a.version == b.version && a.brightness == b.brightness && a.movement_mode == b.movement_mode &&
           a.trackball_responsiveness == b.trackball_responsiveness && a.trackball_speed == b.trackball_speed &&
           a.ui_size == b.ui_size && a.developer_tools_visible == b.developer_tools_visible &&
           a.sound_volume == b.sound_volume && a.music_volume == b.music_volume && a.touch_controls == b.touch_controls &&
           a.keyboard_backlight == b.keyboard_backlight;
}

void settings_codec() {
    const openu5::FrontendSettings d{};
    std::string text;
    check(openu5::encode_settings(d, text), "J1 encode_settings(defaults) succeeds");
    const std::string want = format(
        "{\"version\":1,\"brightness\":80,\"movementMode\":false,\"trackballResponsiveness\":100,\"uiSize\":1,"
        "\"developerToolsVisible\":false,\"soundVolume\":80,\"musicVolume\":80,\"touchControls\":false,"
        "\"trackballSpeed\":%u,\"keyboardBacklight\":%u}",
        unsigned(openu5::kTrackballSpeedDefault), unsigned(openu5::kKeyboardBacklightDefault));
    check(text == want, "J2 the default settings.json text is byte-identical to the T-Deck's format", "got " + text);
    openu5::FrontendSettings back{};
    back.brightness = 10;
    check(openu5::decode_settings(text, back) && same(back, d), "J3 decode(encode(defaults)) == defaults", describe(back));

    openu5::FrontendSettings c{};
    c.brightness = 40;
    c.movement_mode = true;
    c.trackball_responsiveness = 225;
    c.trackball_speed = openu5::kTrackballSpeedMax;
    c.ui_size = 2;
    c.developer_tools_visible = true;
    c.sound_volume = 0;
    c.music_volume = 100;
    c.touch_controls = true;
    c.keyboard_backlight = uint8_t(openu5::kKeyboardBacklightLevels - 1);
    std::string ctext;
    openu5::FrontendSettings cback{};
    check(openu5::encode_settings(c, ctext) && openu5::decode_settings(ctext, cback) && same(cback, c),
          "J4 a non-default document round-trips field for field", describe(cback) + " from " + ctext);

    // A document as an older T-Deck firmware wrote it: no trackballSpeed /
    // keyboardBacklight keys, the Alpha preset responsiveness value 2.
    openu5::FrontendSettings old{};
    const bool old_ok = openu5::decode_settings(
        "{\"version\":1,\"brightness\":60,\"movementMode\":true,\"trackballResponsiveness\":2,\"uiSize\":0,"
        "\"developerToolsVisible\":false,\"soundVolume\":30,\"musicVolume\":50,\"touchControls\":false}",
        old);
    check(old_ok && old.brightness == 60 && old.movement_mode && old.trackball_responsiveness == 200 &&
              old.trackball_speed == openu5::kTrackballSpeedDefault && old.keyboard_backlight == openu5::kKeyboardBacklightDefault &&
              old.ui_size == 0 && old.sound_volume == 30 && old.music_volume == 50,
          "J5 an older card's settings.json (no new keys, preset 2) loads with the migrations", describe(old));

    // Refusals keep the caller's settings untouched.
    const char *bad[] = {
        "{\"version\":2,\"brightness\":60,\"movementMode\":true,\"trackballResponsiveness\":100,\"uiSize\":0,"
        "\"developerToolsVisible\":false,\"soundVolume\":30,\"musicVolume\":50,\"touchControls\":false}",
        "{\"version\":1,\"brightness\":5,\"movementMode\":true,\"trackballResponsiveness\":100,\"uiSize\":0,"
        "\"developerToolsVisible\":false,\"soundVolume\":30,\"musicVolume\":50,\"touchControls\":false}",
        "{\"version\":1,\"brightness\":60,\"movementMode\":1,\"trackballResponsiveness\":100,\"uiSize\":0,"
        "\"developerToolsVisible\":false,\"soundVolume\":30,\"musicVolume\":50,\"touchControls\":false}",
        "{\"version\":1,\"brightness\":60.5,\"movementMode\":true,\"trackballResponsiveness\":100,\"uiSize\":0,"
        "\"developerToolsVisible\":false,\"soundVolume\":30,\"musicVolume\":50,\"touchControls\":false}",
        "{\"version\":1}",
        "not json",
        "",
    };
    int refused = 0;
    for (const char *doc : bad) {
        openu5::FrontendSettings keep = c;
        if (!openu5::decode_settings(doc, keep) && same(keep, c)) ++refused;
    }
    check(refused == int(sizeof bad / sizeof bad[0]),
          format("J6 %d invalid documents (version 2, brightness 5, a non-bool flag, a fractional value, missing keys, "
                 "not JSON, empty) are refused and leave the settings untouched",
                 int(sizeof bad / sizeof bad[0])),
          format("%d refused", refused));

    // Unknown keys are dropped (settings_document writes only its own).
    openu5::FrontendSettings extra{};
    std::string extra_text;
    const bool extra_ok = openu5::decode_settings(
        "{\"windowsOnly\":true,\"version\":1,\"brightness\":90,\"movementMode\":false,\"trackballResponsiveness\":100,"
        "\"uiSize\":1,\"developerToolsVisible\":false,\"soundVolume\":80,\"musicVolume\":80,\"touchControls\":false}",
        extra);
    check(extra_ok && extra.brightness == 90 && openu5::encode_settings(extra, extra_text) &&
              extra_text.find("windowsOnly") == std::string::npos,
          "J7 unknown keys are accepted on read and not written back");
}

} // namespace

int main() {
    std::printf("uv_save_layout_test: the 32-byte save commit record and settings.json under MSVC\n");
    commit_record();
    settings_codec();
    return uvtest::finish("uv_save_layout_test");
}
