// Ultima V Native for Windows -- uv::DesktopSettings (ultima5\windows.json).
//
// The encoder/decoder pair, the defaults, windowScale clamping, malformed
// documents, unknown keys, and save/load through real files in a unique temp
// directory -- one ASCII path and one with non-ASCII characters (a user
// profile like C:\Users\Zoë is common), cleaned up afterwards.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <windows.h>

#include "app/desktop_settings.h"
#include "test_check.h"

using uv::DesktopSettings;
using uv::InterfaceMode;
using uv::Scaling;
using uvtest::check;
using uvtest::format;

namespace {

std::string describe(const DesktopSettings &s) {
    return format("{mode=%s scale=%u fullscreen=%d scaling=%s vsync=%d}", uv::interface_mode_name(s.interface_mode),
                  unsigned(s.window_scale), int(s.fullscreen), uv::scaling_name(s.scaling), int(s.vsync));
}

bool same(const DesktopSettings &a, const DesktopSettings &b) {
    return a.interface_mode == b.interface_mode && a.window_scale == b.window_scale && a.fullscreen == b.fullscreen &&
           a.scaling == b.scaling && a.vsync == b.vsync;
}

DesktopSettings make(InterfaceMode m, uint8_t scale, bool fullscreen, Scaling scaling, bool vsync) {
    DesktopSettings s;
    s.interface_mode = m;
    s.window_scale = scale;
    s.fullscreen = fullscreen;
    s.scaling = scaling;
    s.vsync = vsync;
    return s;
}

/** A unique, empty directory under %TEMP%: <temp>\<leaf>-<pid>-<tick>. */
std::filesystem::path unique_temp_dir(const std::wstring &leaf) {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH + 1, temp);
    std::filesystem::path base = n ? std::filesystem::path(std::wstring(temp, n)) : std::filesystem::temp_directory_path();
    std::filesystem::path dir =
        base / (leaf + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string read_all(const std::filesystem::path &p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_all(const std::filesystem::path &p, const std::string &text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(text.data(), std::streamsize(text.size()));
}

void defaults_and_names() {
    const DesktopSettings d;
    check(d.interface_mode == InterfaceMode::Original && d.window_scale == uv::kAutoWindowScale && !d.fullscreen &&
              d.scaling == Scaling::Framed && d.vsync,
          "S1 defaults: Original, automatic window scale, windowed, Framed scaling, vsync on", describe(d));
    check(uv::kMinWindowScale == 1 && uv::kMaxWindowScale == 8 && uv::kAutoWindowScale == 0,
          "S2 window scale range is 1..8; 0 is automatic");
    check(std::string(uv::interface_mode_name(InterfaceMode::Original)) == "Original" &&
              std::string(uv::interface_mode_name(InterfaceMode::Enhanced)) == "Enhanced",
          "S3 interface_mode_name: Original / Enhanced");
    check(std::string(uv::scaling_name(Scaling::Integer)) == "Integer" && std::string(uv::scaling_name(Scaling::Fit)) == "Fit" &&
              std::string(uv::scaling_name(Scaling::Framed)) == "Framed",
          "S4 scaling_name: Integer / Fit / Framed");
    const std::string text = uv::encode_desktop_settings(d);
    const std::string want =
        "{\"version\":1,\"interfaceMode\":\"Original\",\"fullscreen\":false,\"scaling\":\"Framed\",\"vsync\":true}\n";
    check(text == want, "S5 the default document is exactly the compact JSON windows.json holds (no windowScale: automatic)",
          "got " + text);
    DesktopSettings chosen = d;
    chosen.window_scale = 4;
    const std::string with_scale = uv::encode_desktop_settings(chosen);
    check(with_scale ==
              "{\"version\":1,\"interfaceMode\":\"Original\",\"windowScale\":4,\"fullscreen\":false,\"scaling\":\"Framed\",\"vsync\":true}\n",
          "S5b a chosen scale is written in the first build's place", "got " + with_scale);
}

void round_trips() {
    int good = 0, total = 0;
    std::string first_bad;
    for (InterfaceMode m : {InterfaceMode::Original, InterfaceMode::Enhanced})
        for (uint8_t scale = uv::kAutoWindowScale; scale <= uv::kMaxWindowScale; ++scale)
            for (bool fs : {false, true})
                for (Scaling sc : {Scaling::Integer, Scaling::Fit, Scaling::Framed})
                    for (bool vs : {false, true}) {
                        const DesktopSettings in = make(m, scale, fs, sc, vs);
                        DesktopSettings out = make(InterfaceMode::Original, 1, !fs, sc == Scaling::Fit ? Scaling::Integer : Scaling::Fit, !vs);
                        const bool ok = uv::decode_desktop_settings(uv::encode_desktop_settings(in), out) && same(in, out);
                        ++total;
                        if (ok) ++good;
                        else if (first_bad.empty()) first_bad = describe(in) + " -> " + describe(out);
                    }
    check(good == total && total == 2 * 9 * 2 * 3 * 2,
          format("S6 encode -> decode round-trips every combination (%d/%d: 2 modes x (automatic + 8 scales) x fullscreen x 3 scalings x vsync)",
                 good, total),
          first_bad);
}

void clamping() {
    struct Row {
        const char *value;
        unsigned want;
    };
    const Row rows[] = {{"0", 1},   {"-1", 1},  {"-1000", 1}, {"9", 8},     {"255", 8},  {"256", 8},
                        {"1000", 8}, {"1", 1},  {"8", 8},     {"2.9", 2},   {"5", 5},    {"7.999", 7},
                        {"1e3", 8},  {"-0", 1}, {"0.5", 1},   {"8.5", 8}};
    for (const Row &r : rows) {
        DesktopSettings s;
        const std::string doc = std::string("{\"windowScale\":") + r.value + "}";
        const bool ok = uv::decode_desktop_settings(doc, s);
        check(ok && s.window_scale == r.want, format("S7 windowScale %s -> %u (clamped into 1..8)", r.value, r.want),
              format("ok=%d scale=%u", int(ok), unsigned(s.window_scale)));
    }
}

void malformed_and_unknown() {
    const DesktopSettings sentinel = make(InterfaceMode::Enhanced, 6, true, Scaling::Fit, false);
    struct Row {
        const char *name;
        std::string text;
    };
    const Row bad[] = {
        {"empty text", ""},
        {"whitespace", "   \n"},
        {"truncated object", "{\"interfaceMode\":\"Enhanced\""},
        {"missing value", "{\"windowScale\":}"},
        {"trailing comma", "{\"windowScale\":4,}"},
        {"array, not object", "[1,2,3]"},
        {"string, not object", "\"Enhanced\""},
        {"number, not object", "4"},
        {"null", "null"},
        {"single quotes", "{'interfaceMode':'Enhanced'}"},
        {"garbage", "interfaceMode=Enhanced"},
        {"two documents", "{} {}"},
    };
    for (const Row &r : bad) {
        DesktopSettings s = sentinel;
        const bool ok = uv::decode_desktop_settings(r.text, s);
        check(!ok && same(s, sentinel), format("S8 malformed (%s) is rejected and leaves the output untouched", r.name),
              format("ok=%d out=%s", int(ok), describe(s).c_str()));
    }
    {
        DesktopSettings s = sentinel;
        const bool ok = uv::decode_desktop_settings(
            "{\"future\":{\"nested\":[1,2,{\"x\":null}]},\"interfaceMode\":\"Enhanced\",\"windowScale\":5,\"color\":\"blue\","
            "\"version\":99}",
            s);
        check(ok && s.interface_mode == InterfaceMode::Enhanced && s.window_scale == 5 && !s.fullscreen &&
                  s.scaling == Scaling::Framed && s.vsync,
              "S9 unknown keys (and an unknown version) are ignored; absent keys take the defaults", describe(s));
    }
    {
        DesktopSettings s = sentinel;
        const bool ok = uv::decode_desktop_settings("{}", s);
        check(ok && same(s, DesktopSettings{}), "S10 an empty object decodes to the defaults (not the previous contents)",
              describe(s));
    }
    {
        // Wrong types keep the default for that key only.
        DesktopSettings s;
        const bool ok = uv::decode_desktop_settings(
            "{\"interfaceMode\":1,\"windowScale\":\"5\",\"fullscreen\":\"yes\",\"scaling\":true,\"vsync\":0}", s);
        check(ok && same(s, DesktopSettings{}), "S11 values of the wrong JSON type keep each key's default", describe(s));
    }
    {
        DesktopSettings s;
        const bool ok = uv::decode_desktop_settings("{\"interfaceMode\":\"enhanced\",\"scaling\":\"fit\"}", s);
        check(ok && s.interface_mode == InterfaceMode::Original && s.scaling == Scaling::Framed,
              "S12 names are case-sensitive: \"enhanced\"/\"fit\" are not recognised (defaults)", describe(s));
        DesktopSettings t;
        const bool ok2 = uv::decode_desktop_settings("{\"interfaceMode\":\"Enh\\u0061nced\",\"scaling\":\"Fit\"}", t);
        check(ok2 && t.interface_mode == InterfaceMode::Enhanced && t.scaling == Scaling::Fit,
              "S13 JSON escapes are decoded (\"Enh\\u0061nced\" is Enhanced)", describe(t));
        DesktopSettings u;
        const bool ok3 = uv::decode_desktop_settings("{\"interfaceMode\":\"Enhanced\\u00e9\"}", u);
        check(ok3 && u.interface_mode == InterfaceMode::Original, "S14 a non-ASCII mode name is not Enhanced", describe(u));
    }
    {
        // windows.json files the earlier builds wrote, and future or mistyped scaling names.
        struct Legacy {
            const char *name;
            const char *text;
            Scaling want;
        };
        const Legacy legacy[] = {
            {"the first build's default document (Integer stays Integer)",
             "{\"version\":1,\"interfaceMode\":\"Original\",\"windowScale\":3,\"fullscreen\":false,\"scaling\":\"Integer\",\"vsync\":true}\n",
             Scaling::Integer},
            {"a first-build document with Fit (Fit stays Fit)",
             "{\"version\":1,\"interfaceMode\":\"Enhanced\",\"windowScale\":4,\"fullscreen\":true,\"scaling\":\"Fit\",\"vsync\":false}\n",
             Scaling::Fit},
            {"no scaling key (Framed)", "{\"version\":1,\"windowScale\":2}", Scaling::Framed},
            {"\"Framed\"", "{\"scaling\":\"Framed\"}", Scaling::Framed},
            {"a later build's name, \"Adaptive\" (the default, Framed)", "{\"scaling\":\"Adaptive\"}", Scaling::Framed},
            {"an empty name (Framed)", "{\"scaling\":\"\"}", Scaling::Framed},
        };
        for (const Legacy &l : legacy) {
            DesktopSettings s = sentinel;
            const bool ok = uv::decode_desktop_settings(l.text, s);
            check(ok && s.scaling == l.want, format("S15 %s -> %s", l.name, uv::scaling_name(l.want)), describe(s));
        }
        DesktopSettings first;
        uv::decode_desktop_settings(legacy[0].text, first);
        check(first.window_scale == 3 && first.interface_mode == InterfaceMode::Original && !first.fullscreen && first.vsync,
              "S16 the first build's document keeps its other values", describe(first));
        check(uv::scaling_from_name("Integer") == Scaling::Integer && uv::scaling_from_name("Fit") == Scaling::Fit &&
                  uv::scaling_from_name("Framed") == Scaling::Framed && uv::scaling_from_name("integer") == Scaling::Framed,
              "S17 scaling_from_name inverts scaling_name; anything else is Framed");
        DesktopSettings no_scale;
        no_scale.window_scale = 5;
        check(uv::decode_desktop_settings("{\"version\":1,\"scaling\":\"Framed\"}", no_scale) &&
                  no_scale.window_scale == uv::kAutoWindowScale,
              "S18 no windowScale key: the scale is automatic (derived from the display at start-up)", describe(no_scale));
        check(first.window_scale == 3, "S19 the first build's windowScale 3 is a saved choice, kept as 3", describe(first));
    }
    {
        // A document with a BOM, as Notepad writes it, is not JSON the core parser accepts.
        DesktopSettings s = sentinel;
        const bool ok = uv::decode_desktop_settings("\xef\xbb\xbf{\"interfaceMode\":\"Enhanced\"}", s);
        uvtest::note("a UTF-8 BOM before the object: decode returned %s", ok ? "true (accepted)" : "false (rejected)");
    }
}

void files_in(const std::wstring &leaf, const char *tag) {
    const std::filesystem::path dir = unique_temp_dir(leaf);
    std::error_code ec;
    check(std::filesystem::is_directory(dir, ec), format("F0 %s: temp directory created", tag));
    // A path below a folder that does not exist yet: save creates the parents.
    const std::filesystem::path file = dir / L"ultima5" / L"windows.json";
    const std::wstring wfile = file.wstring();

    DesktopSettings loaded = make(InterfaceMode::Enhanced, 7, true, Scaling::Fit, false);
    check(uv::load_desktop_settings(wfile, loaded) && same(loaded, DesktopSettings{}),
          format("F1 %s: a missing file loads as the defaults (true)", tag), describe(loaded));

    const DesktopSettings custom = make(InterfaceMode::Enhanced, 5, true, Scaling::Fit, false);
    check(uv::save_desktop_settings(wfile, custom), format("F2 %s: save creates the parent folder and writes the file", tag));
    check(std::filesystem::is_regular_file(file, ec), format("F3 %s: windows.json exists after save", tag));
    std::filesystem::path tmp = file;
    tmp += L".tmp";
    check(!std::filesystem::exists(tmp, ec), format("F4 %s: no .tmp file is left behind", tag));
    check(read_all(file) == uv::encode_desktop_settings(custom), format("F5 %s: the file holds exactly the encoded text", tag));

    DesktopSettings back;
    check(uv::load_desktop_settings(wfile, back) && same(back, custom), format("F6 %s: load returns what save wrote", tag),
          describe(back));

    // Saving again replaces the file (MoveFileEx REPLACE_EXISTING).
    const DesktopSettings second = make(InterfaceMode::Original, 2, false, Scaling::Integer, true);
    DesktopSettings back2;
    check(uv::save_desktop_settings(wfile, second) && uv::load_desktop_settings(wfile, back2) && same(back2, second),
          format("F7 %s: a second save replaces the first", tag), describe(back2));

    // A malformed file: load reports false and leaves the defaults.
    write_all(file, "{\"interfaceMode\":\"Enhanced\",");
    DesktopSettings broken = custom;
    const bool ok = uv::load_desktop_settings(wfile, broken);
    check(!ok && same(broken, DesktopSettings{}), format("F8 %s: a malformed file loads false, with the defaults", tag),
          format("ok=%d %s", int(ok), describe(broken).c_str()));

    // An oversized file (more than 64 KiB) is refused before parsing.
    std::string big = "{\"interfaceMode\":\"Enhanced\",\"pad\":\"";
    big.append(70 * 1024, 'x');
    big += "\"}";
    write_all(file, big);
    DesktopSettings huge = custom;
    const bool ok_big = uv::load_desktop_settings(wfile, huge);
    check(!ok_big && same(huge, DesktopSettings{}), format("F9 %s: a file over 64 KiB is refused (defaults)", tag),
          format("ok=%d %s", int(ok_big), describe(huge).c_str()));

    // A hand-edited file with unknown keys and CRLF line ends loads.
    write_all(file, "{\r\n  \"interfaceMode\": \"Enhanced\",\r\n  \"windowScale\": 12,\r\n  \"comment\": \"mine\"\r\n}\r\n");
    DesktopSettings edited;
    check(uv::load_desktop_settings(wfile, edited) && edited.interface_mode == InterfaceMode::Enhanced && edited.window_scale == 8,
          format("F10 %s: a hand-edited file (CRLF, spaces, unknown key, scale 12) loads with the scale clamped to 8", tag),
          describe(edited));

    std::filesystem::remove_all(dir, ec);
    check(!std::filesystem::exists(dir, ec), format("F11 %s: temp directory removed", tag));
}

} // namespace

int main() {
    std::printf("uv_desktop_settings_test: uv::DesktopSettings codec and windows.json files\n");
    defaults_and_names();
    round_trips();
    clamping();
    malformed_and_unknown();
    files_in(L"uv-settings-test", "ASCII path");
    files_in(L"uv-t\u00e9st-\u03a9", "non-ASCII path (uv-t\u00e9st-\u03a9)");
    return uvtest::finish("uv_desktop_settings_test");
}
