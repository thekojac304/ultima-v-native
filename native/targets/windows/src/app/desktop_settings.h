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

// How the 320x240 picture fills the window (app/presentation_layout.h):
// Integer = whole pixels, letterboxed; Fit = the largest 4:3 fit; Framed =
// whole pixels in a thin ring of the game's chrome.
enum class Scaling : uint8_t { Integer = 0, Fit = 1, Framed = 2 };

// The window scale the player has not chosen: the game derives it from the
// display's scale (app/presentation_layout.h, default_window_scale) and
// windows.json leaves "windowScale" out.
constexpr uint8_t kAutoWindowScale = 0;

// Original mode's game screen: Dos = the 1988 game's own 320x200 one
// (Board::set_dos_layout; the default), Handheld = the T-Deck's 320x240 one
// that Enhanced always shows.
enum class OriginalScreen : uint8_t { Dos = 0, Handheld = 1 };

struct DesktopSettings {
    InterfaceMode interface_mode = InterfaceMode::Original;
    uint8_t window_scale = kAutoWindowScale;  // the picture's scale N in a window (1..8), or automatic
    bool fullscreen = false;
    Scaling scaling = Scaling::Framed;
    bool vsync = true;
    OriginalScreen original_screen = OriginalScreen::Dos;
};

constexpr uint8_t kMinWindowScale = 1, kMaxWindowScale = 8;

const char *interface_mode_name(InterfaceMode);
/** "Framed", "Integer" or "Fit": the names windows.json holds. */
const char *scaling_name(Scaling);
/** The inverse; an unknown name is the default, Framed. */
Scaling scaling_from_name(const std::string &name);
/** "DOS" or "Handheld": the names windows.json and --original-screen take. */
const char *original_screen_name(OriginalScreen);
/** The inverse; false (and `out` unchanged) for an unknown name. */
bool original_screen_from_name(const std::string &name, OriginalScreen &out);

/** Reads `path`; a missing or malformed file (or key) keeps the defaults. False only on a malformed file. */
bool load_desktop_settings(const std::wstring &path, DesktopSettings &out);
bool save_desktop_settings(const std::wstring &path, const DesktopSettings &settings);
/** The JSON text save_desktop_settings writes (tests). */
std::string encode_desktop_settings(const DesktopSettings &settings);
bool decode_desktop_settings(const std::string &text, DesktopSettings &out);

} // namespace uv
