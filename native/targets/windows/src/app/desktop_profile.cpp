#include "app/desktop_profile.h"

#include <algorithm>
#include <cstdio>

#include "build_info.h"

namespace uv {
namespace {
using openu5::SettingsRowId;

// Window sizes the Display row steps through, then fullscreen.
constexpr uint8_t kDisplayScales[] = {2, 3, 4, 5, 6};
constexpr int kDisplayStates = int(sizeof(kDisplayScales)) + 1; // + fullscreen

char g_banner[64];
} // namespace

size_t DesktopProfile::rows_for(InterfaceMode mode, SettingsRowId *out, size_t capacity) {
    const SettingsRowId original[] = {SettingsRowId::Platform0, SettingsRowId::Platform1, SettingsRowId::Platform2,
                                      SettingsRowId::TextSize,  SettingsRowId::SfxVolume, SettingsRowId::MusicVolume};
    size_t n = 0;
    for (const auto row : original)
        if (n < capacity) out[n++] = row;
    // WASD Movement Mode is an Enhanced convenience; its default lives in settings.json.
    if (mode == InterfaceMode::Enhanced && n < capacity) out[n++] = SettingsRowId::Movement;
    return n;
}

size_t DesktopProfile::settings_rows(void *context, SettingsRowId *out, size_t capacity) {
    const auto *self = static_cast<const DesktopProfile *>(context);
    return rows_for(self->hooks_.settings->interface_mode, out, capacity);
}

bool DesktopProfile::enhanced_rows(void *context) {
    const auto *self = static_cast<const DesktopProfile *>(context);
    return self->hooks_.settings->interface_mode == InterfaceMode::Enhanced;
}

// The 1988 behaviours Original restores: typed spell names, (Q)uit & Save.
bool DesktopProfile::original_mode(void *context) { return !enhanced_rows(context); }

const char *DesktopProfile::interface_label(void *context, char *out, size_t capacity) {
    const auto *self = static_cast<const DesktopProfile *>(context);
    const bool enhanced = self->hooks_.settings->interface_mode == InterfaceMode::Enhanced;
    std::snprintf(out, capacity, "Interface: %s", enhanced ? "Enhanced" : "Original");
    return enhanced ? "Extras on: Difficulty, Cheats, F5/F9, WASD" : "The 1988 keys and rules; no extras";
}

bool DesktopProfile::interface_adjust(void *context, int) {
    auto *self = static_cast<DesktopProfile *>(context);
    auto &mode = self->hooks_.settings->interface_mode;
    mode = mode == InterfaceMode::Enhanced ? InterfaceMode::Original : InterfaceMode::Enhanced;
    if (self->hooks_.changed) self->hooks_.changed(self->hooks_.context, Change::Interface);
    return true;
}

const char *DesktopProfile::display_label(void *context, char *out, size_t capacity) {
    const auto *self = static_cast<const DesktopProfile *>(context);
    const auto &s = *self->hooks_.settings;
    if (s.fullscreen) std::snprintf(out, capacity, "Display: Fullscreen");
    else std::snprintf(out, capacity, "Display: Window %ux", unsigned(s.window_scale));
    return "Left/right: window size or fullscreen (Alt+Enter)";
}

bool DesktopProfile::display_adjust(void *context, int delta) {
    auto *self = static_cast<DesktopProfile *>(context);
    auto &s = *self->hooks_.settings;
    int state = kDisplayStates - 1; // fullscreen
    if (!s.fullscreen) {
        state = 0;
        for (int i = 0; i < int(sizeof(kDisplayScales)); ++i)
            if (kDisplayScales[i] <= s.window_scale) state = i;
    }
    state = (state + (delta < 0 ? kDisplayStates - 1 : 1)) % kDisplayStates;
    s.fullscreen = state == kDisplayStates - 1;
    if (!s.fullscreen) s.window_scale = kDisplayScales[state];
    if (self->hooks_.changed) self->hooks_.changed(self->hooks_.context, Change::Display);
    return true;
}

const char *DesktopProfile::scaling_label(void *context, char *out, size_t capacity) {
    const auto *self = static_cast<const DesktopProfile *>(context);
    const bool fit = self->hooks_.settings->scaling == Scaling::Fit;
    std::snprintf(out, capacity, "Scaling: %s", fit ? "Fit window" : "Whole pixels");
    return fit ? "Fills the window; pixels may differ in size" : "Every pixel the same size; may letterbox";
}

bool DesktopProfile::scaling_adjust(void *context, int) {
    auto *self = static_cast<DesktopProfile *>(context);
    auto &scaling = self->hooks_.settings->scaling;
    scaling = scaling == Scaling::Fit ? Scaling::Integer : Scaling::Fit;
    if (self->hooks_.changed) self->hooks_.changed(self->hooks_.context, Change::Scaling);
    return true;
}

void DesktopProfile::install(const Hooks &hooks) {
    hooks_ = hooks;
    std::snprintf(g_banner, sizeof(g_banner), "%s for Windows %s", build::kProductName, build::kVersion);
    openu5::PlatformProfile p{};
    p.back_key = "Esc";
    p.move_keys = "Arrows";
    p.menu_key = "F10";
    p.empty_storage = "No saved journey in the data folder";
    p.import_folder = "ultima5\\import";
    p.export_folder = "ultima5\\export";
    p.storage_write_error = "disk write error";
    p.storage_short = "disk";
    p.banner = g_banner;
    // Printed once, when the runtime starts: the same in both interface modes.
    p.banner_keys = "Esc back  F10 menu  Alt+Enter fullscreen";
    p.settings_rows = &DesktopProfile::settings_rows;
    p.settings_context = this;
    p.platform_rows[0] = {&DesktopProfile::interface_label, &DesktopProfile::interface_adjust, this};
    p.platform_rows[1] = {&DesktopProfile::display_label, &DesktopProfile::display_adjust, this};
    p.platform_rows[2] = {&DesktopProfile::scaling_label, &DesktopProfile::scaling_adjust, this};
    p.enhanced_rows = &DesktopProfile::enhanced_rows;
    p.enhanced_context = this;
    // Original types the spell's syllable initials, as in 1988; Enhanced keeps
    // the spell list (a quick-select convenience).
    p.typed_spell_names = &DesktopProfile::original_mode;
    p.typed_spell_context = this;
    // Original restores the 1988 (Q)uit & Save; Enhanced has F5/F9 and the menu.
    p.quit_save_key = &DesktopProfile::original_mode;
    p.quit_save_context = this;
    // Original follows the 1988 interaction model: roster pickers, console
    // prompts, the DOS key rules (ORIGINAL_MODE_FIDELITY.md); Enhanced keeps
    // the handheld's list panels and key legends.
    p.dos_interface = &DesktopProfile::original_mode;
    p.dos_interface_context = this;
    openu5::set_platform_profile(p);
}

} // namespace uv
