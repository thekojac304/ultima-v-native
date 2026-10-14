#include "app/desktop_settings.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include <windows.h>

#include "openu5/save_json.h"

namespace uv {
namespace {
using openu5::save::Json;

std::string u16_to_ascii(const std::u16string &s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t c : s) out.push_back(c < 0x80 ? char(c) : '?');
    return out;
}
} // namespace

const char *interface_mode_name(InterfaceMode mode) {
    return mode == InterfaceMode::Enhanced ? "Enhanced" : "Original";
}

const char *scaling_name(Scaling scaling) { return scaling == Scaling::Fit ? "Fit" : "Integer"; }

std::string encode_desktop_settings(const DesktopSettings &s) {
    Json doc = Json::object();
    doc["version"] = Json(1);
    doc["interfaceMode"] = Json(interface_mode_name(s.interface_mode));
    doc["windowScale"] = Json(int(s.window_scale));
    doc["fullscreen"] = Json(s.fullscreen);
    doc["scaling"] = Json(scaling_name(s.scaling));
    doc["vsync"] = Json(s.vsync);
    return openu5::save::write_json(doc) + "\n";
}

bool decode_desktop_settings(const std::string &text, DesktopSettings &out) {
    Json doc;
    if (openu5::save::parse_json(text, doc) != openu5::save::JsonError::None || doc.kind != Json::Object) return false;
    DesktopSettings s;
    if (doc.has("interfaceMode") && doc["interfaceMode"].kind == Json::String)
        s.interface_mode = u16_to_ascii(doc["interfaceMode"].string) == "Enhanced" ? InterfaceMode::Enhanced
                                                                                    : InterfaceMode::Original;
    if (doc.has("windowScale") && doc["windowScale"].kind == Json::Number)
        s.window_scale = uint8_t(std::clamp<int64_t>(doc["windowScale"].integer(3), kMinWindowScale, kMaxWindowScale));
    if (doc.has("fullscreen") && doc["fullscreen"].kind == Json::Bool) s.fullscreen = doc["fullscreen"].truth();
    if (doc.has("scaling") && doc["scaling"].kind == Json::String)
        s.scaling = u16_to_ascii(doc["scaling"].string) == "Fit" ? Scaling::Fit : Scaling::Integer;
    if (doc.has("vsync") && doc["vsync"].kind == Json::Bool) s.vsync = doc["vsync"].truth();
    out = s;
    return true;
}

bool load_desktop_settings(const std::wstring &path, DesktopSettings &out) {
    out = DesktopSettings{};
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) return true; // no file yet: the defaults
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.size() > 64 * 1024) return false;
    return decode_desktop_settings(text, out);
}

bool save_desktop_settings(const std::wstring &path, const DesktopSettings &settings) {
    const std::string text = encode_desktop_settings(settings);
    const std::filesystem::path target(path);
    std::error_code ec;
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    std::filesystem::path tmp = target;
    tmp += L".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(text.data(), std::streamsize(text.size()));
        out.flush();
        if (!out) return false;
    }
    return MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

} // namespace uv
