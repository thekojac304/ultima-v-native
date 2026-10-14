#include "openu5/platform_profile.h"

#include <cstring>

namespace openu5 {
namespace {
PlatformProfile g_profile{};
constexpr SettingsRowId kTdeckRows[] = {SettingsRowId::Brightness, SettingsRowId::Movement,  SettingsRowId::Trackball,
                                        SettingsRowId::TextSize,   SettingsRowId::SfxVolume, SettingsRowId::MusicVolume,
                                        SettingsRowId::KeyboardLight};
} // namespace

const PlatformProfile &platform_profile() { return g_profile; }

void set_platform_profile(const PlatformProfile &profile) { g_profile = profile; }

size_t platform_settings_rows(SettingsRowId *out, size_t capacity) {
    size_t n = 0;
    if (g_profile.settings_rows) {
        n = g_profile.settings_rows(g_profile.settings_context, out, capacity);
        if (n > capacity) n = capacity;
        if (n) return n;
    }
    for (const auto row : kTdeckRows)
        if (n < capacity) out[n++] = row;
    return n;
}

size_t platform_format(char *out, size_t capacity, const char *text) {
    if (!out || !capacity) return 0;
    size_t n = 0;
    auto put = [&](const char *s) {
        for (; s && *s && n + 1 < capacity; ++s) out[n++] = *s;
    };
    struct Token { const char *name; const char *value; };
    const Token tokens[] = {{"{back}", g_profile.back_key},      {"{move}", g_profile.move_keys},
                            {"{menu}", g_profile.menu_key},      {"{import}", g_profile.import_folder},
                            {"{export}", g_profile.export_folder}, {"{storage}", g_profile.storage_short}};
    for (const char *p = text ? text : ""; *p && n + 1 < capacity;) {
        bool replaced = false;
        if (*p == '{') {
            for (const auto &t : tokens) {
                const size_t len = std::strlen(t.name);
                if (std::strncmp(p, t.name, len) == 0) {
                    put(t.value);
                    p += len;
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) out[n++] = *p++;
    }
    out[n] = 0;
    return n;
}

bool platform_typed_spell_names() {
    return g_profile.typed_spell_names && g_profile.typed_spell_names(g_profile.typed_spell_context);
}

bool platform_quit_save_key() {
    return g_profile.quit_save_key && g_profile.quit_save_key(g_profile.quit_save_context);
}

bool platform_enhanced_rows() {
    return !g_profile.enhanced_rows || g_profile.enhanced_rows(g_profile.enhanced_context);
}

} // namespace openu5
