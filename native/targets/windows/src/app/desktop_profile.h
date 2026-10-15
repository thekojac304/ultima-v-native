#pragma once
// Ultima V Native for Windows: the desktop's platform profile
// (native/core/include/openu5/platform_profile.h).
//
// The shared menus and prompts name the T-Deck's keys ("Mic returns",
// "Trackball: choose") and its SD card. On Windows they name the desktop's
// (Esc, the arrow keys, F10) and its data folder; the Settings page shows the
// desktop rows -- Interface (Original/Enhanced), Display (window size or
// fullscreen) and Scaling -- instead of the T-Deck's hardware rows
// (Brightness, Trackball speed, Keyboard Backlight); WASD "Movement default"
// is listed in Enhanced only; and the System Menu lists Difficulty and Cheats
// only in Enhanced.
#include <cstddef>

#include "app/desktop_settings.h"
#include "app/dos_text.h"
#include "openu5/platform_profile.h"

namespace uv {

class DesktopProfile {
  public:
    enum class Change { Interface, Display, Scaling };
    struct Hooks {
        DesktopSettings *settings = nullptr;
        void *context = nullptr;
        /** A desktop row changed `settings`; apply and persist it. */
        void (*changed)(void *context, Change what) = nullptr;
        /** The largest window scale the display holds now; the Display row steps 1..it, then Fullscreen (null = 8). */
        uint8_t (*largest_window_scale)(void *context) = nullptr;
        /** The DOS name tables Original mode's list panels print (null = none). */
        const DosTextTables *dos_text = nullptr;
    };

    /** Installs the profile (call again after the interface mode changed: the banner keys follow it). */
    void install(const Hooks &hooks);

    // Exposed for tests: the rows the Settings page shows in each mode.
    static size_t rows_for(InterfaceMode mode, openu5::SettingsRowId *out, size_t capacity);

  private:
    static size_t settings_rows(void *context, openu5::SettingsRowId *out, size_t capacity);
    static bool enhanced_rows(void *context);
    static bool original_mode(void *context);
    static const char *interface_label(void *context, char *out, size_t capacity);
    static bool interface_adjust(void *context, int delta);
    static const char *display_label(void *context, char *out, size_t capacity);
    static bool display_adjust(void *context, int delta);
    static const char *scaling_label(void *context, char *out, size_t capacity);
    static bool scaling_adjust(void *context, int delta);
    static const char *dos_text(void *context, uint8_t table, int index);
    static bool dos_screen(void *context);

    Hooks hooks_{};
};

} // namespace uv
