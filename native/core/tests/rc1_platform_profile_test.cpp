#include "openu5/platform_profile.h"
#include <cstdio>
#include <cstring>

namespace {
int calls = 0;
int failures = 0;
bool enabled(void *) { ++calls; return true; }
const char *text(void *, uint8_t, int) { ++calls; return "probe"; }
void check(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); ++failures; }
}
}
int main() {
    using namespace openu5;
#ifdef EXPECT_FIXED_HANDHELD
    constexpr bool fixed = true;
#else
    constexpr bool fixed = false;
#endif
    check(!platform_typed_spell_names() && !platform_quit_save_key() &&
          !platform_dos_interface() && !platform_dos_screen() &&
          !platform_dos_text(DosText::Literal, 0), "default handheld behavior");
    SettingsRowId rows[kMaxSettingsRows]{};
    check(platform_settings_rows(rows, kMaxSettingsRows) == 7 &&
          rows[0] == SettingsRowId::Brightness && rows[6] == SettingsRowId::KeyboardLight,
          "all seven existing device settings remain");
    check(platform_enhanced_rows(), "device difficulty and cheats remain available");
    PlatformProfile p{};
    p.typed_spell_names = enabled; p.quit_save_key = enabled;
    p.dos_interface = enabled; p.dos_screen = enabled; p.dos_text = text;
    set_platform_profile(p);
    check(platform_typed_spell_names() == !fixed, "typed spell capability");
    check(platform_quit_save_key() == !fixed, "quit/save capability");
    check(platform_dos_interface() == !fixed, "DOS interaction capability");
    check(platform_dos_screen() == !fixed, "DOS screen capability");
    const char *result = platform_dos_text(DosText::Literal, 7);
    check(fixed ? result == nullptr : result && std::strcmp(result, "probe") == 0,
          "DOS data capability");
    check(kPlatformSupportsDosPresentation == !fixed, "Board presentation capability");
    check(calls == (fixed ? 0 : 5), "desktop callbacks excluded only in fixed handheld build");
    char formatted[64]{};
    platform_format(formatted, sizeof(formatted), "{back}/{move}/{storage}");
    check(std::strcmp(formatted, "Mic/Trackball/SD") == 0, "shared handheld wording unchanged");
    set_platform_profile(PlatformProfile{});
    return failures ? 1 : 0;
}
