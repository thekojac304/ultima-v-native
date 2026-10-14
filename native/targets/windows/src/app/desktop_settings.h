#pragma once
// Ultima V Native for Windows: the desktop-only preferences.
//
// The game's own settings (volumes, text size, brightness, ...) stay in
// ultima5\settings.json, the file the T-Deck writes, so a card's settings and
// a desktop's are interchangeable. What only a desktop has -- the interface
// mode, the window and the display scaling -- lives beside it in
// ultima5\windows.json (written with the core's JSON writer, through a
// temporary file and a replace, like settings.json).
#include <cstdint>
#include <string>

namespace uv {

// Original: the preservation-oriented experience -- the DOS keyboard, the
// 1988 rules (the System Menu's Difficulty and Cheats are hidden), no
// convenience shortcuts. Enhanced: the project's optional enhancements --
// Difficulty and Cheats, WASD Movement Mode, quick save/load keys, the
// Developer menu. The journey's rules themselves always travel in its save
// (the sidecar's "enhanced" key), whatever the interface mode.
enum class InterfaceMode : uint8_t { Original = 0, Enhanced = 1 };

enum class Scaling : uint8_t { Integer = 0, Fit = 1 };

struct DesktopSettings {
    InterfaceMode interface_mode = InterfaceMode::Original;
    uint8_t window_scale = 3;  // 320x240 x N for the windowed size (1..8)
    bool fullscreen = false;
    Scaling scaling = Scaling::Integer;
    bool vsync = true;
};

constexpr uint8_t kMinWindowScale = 1, kMaxWindowScale = 8;

const char *interface_mode_name(InterfaceMode);
const char *scaling_name(Scaling);

/** Reads `path`; a missing or malformed file (or key) keeps the defaults. False only on a malformed file. */
bool load_desktop_settings(const std::wstring &path, DesktopSettings &out);
bool save_desktop_settings(const std::wstring &path, const DesktopSettings &settings);
/** The JSON text save_desktop_settings writes (tests). */
std::string encode_desktop_settings(const DesktopSettings &settings);
bool decode_desktop_settings(const std::string &text, DesktopSettings &out);

} // namespace uv
