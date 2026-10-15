#pragma once
#include <cstddef>
#include <cstdint>

namespace openu5 {

// Windows port (WINDOWS_PORT_PLAN.md). What differs between the platforms the
// same user interface runs on: the names of the keys the menus and prompts
// mention, where the player's files live, which Settings rows exist, and
// whether the System Menu lists the Enhanced rows. The defaults are the
// T-Deck's, byte for byte -- every string below is the literal the T-Deck has
// always shown -- so a build that never calls set_platform_profile() (the
// firmware) behaves exactly as before. A desktop build installs its own
// profile once, at start-up, before any view is composed.

// The Settings page's rows. The first seven are the T-Deck's, in its order
// (SystemMenuSession::SettingsRow has the same values); a platform may also
// show up to three rows of its own (PlatformRow).
enum class SettingsRowId : uint8_t {
    Brightness, Movement, Trackball, TextSize, SfxVolume, MusicVolume, KeyboardLight,
    Platform0, Platform1, Platform2,
};
constexpr size_t kMaxSettingsRows = 10;
constexpr size_t kPlatformRowCount = 3;

/** A Settings row the platform provides (e.g. the desktop's interface mode). */
struct PlatformRow {
    /** Writes the row's text; returns the footer while it is selected (nullptr = the page's own). */
    const char *(*label)(void *context, char *out, size_t capacity) = nullptr;
    /** Left (-1), right or Enter (+1): change the value; true = it changed. */
    bool (*adjust)(void *context, int delta) = nullptr;
    void *context = nullptr;
};

struct PlatformProfile {
    // --- Key and storage words -------------------------------------------
    const char *back_key = "Mic";             // the key that backs out ("Mic returns", "Mic: leave")
    const char *move_keys = "Trackball";      // what moves a cursor ("Trackball: choose")
    const char *menu_key = "Alt+M";           // the System Menu key ("Alt+M or Mic: resume")
    const char *empty_storage = "No saved journey on this card";
    const char *import_folder = "/ultima5/import";
    const char *export_folder = "/ultima5/export";
    const char *storage_write_error = "SD card write error";
    const char *storage_short = "SD";         // "could not be read from SD"
    // The two lines a new or loaded journey opens its console with.
    const char *banner = "OpenU5 T-Deck Alpha 2.0 Frontend";
    const char *banner_keys = "Mic back  Alt+D debug  Alt+S save  Alt+L load";

    // --- Settings rows ----------------------------------------------------
    // Writes the rows to show, in order; returns how many. Null = the T-Deck's seven.
    size_t (*settings_rows)(void *context, SettingsRowId *out, size_t capacity) = nullptr;
    void *settings_context = nullptr;
    PlatformRow platform_rows[kPlatformRowCount]{};

    // --- Spell names -------------------------------------------------------
    // Whether Cast and Mix ask for the spell the 1988 way -- typed syllable
    // initials, CAST2.OVL:0x00DE (openu5/spell_entry.h) -- instead of the
    // T-Deck's spell list. Null = the list (the T-Deck).
    bool (*typed_spell_names)(void *context) = nullptr;
    void *typed_spell_context = nullptr;
    // Whether Q is the 1988 (Q)uit & Save (CAST2.OVL:0x10FE: "Quit:", then
    // "Save game? "). Null = not a command (the T-Deck saves from its System
    // Menu instead, declared adaptation A-5).
    bool (*quit_save_key)(void *context) = nullptr;
    void *quit_save_context = nullptr;

    // --- The 1988 interaction model ------------------------------------------
    // Whether the interface follows the DOS game's own interaction model
    // (native/targets/windows/ORIGINAL_MODE_FIDELITY.md): members are chosen
    // on the roster by a reverse-video row under a "Select:" caption instead
    // of a list panel; prompts are typed inline on the console with the 1988
    // key rules (ESC clears a typed line, a getdir ignores ESC and takes Space
    // as "Pass", a Y/N question takes only Y or N) instead of the device's
    // context bar and its key legends; and the prompts, echoes and cancel
    // strings are the binary's own. Null = the T-Deck's handheld interface.
    bool (*dos_interface)(void *context) = nullptr;
    void *dos_interface_context = nullptr;
    // The DOS game's own short names and glyphs for its right-panel lists
    // (the (U)se and (R)eady pickers, Z-stats' lists, Mix's reagents, the
    // blacksmith's "Arms" window), read from the player's game data by the
    // platform: see DosText. Null, or a null result, = no DOS name; the
    // runtime then keeps the device's own name (the T-Deck never asks).
    const char *(*dos_text)(void *context, uint8_t table, int index) = nullptr;
    void *dos_text_context = nullptr;

    // --- System Menu --------------------------------------------------------
    // Whether the root lists Difficulty and Cheats. Null = always (the T-Deck).
    bool (*enhanced_rows)(void *context) = nullptr;
    void *enhanced_context = nullptr;
};

const PlatformProfile &platform_profile();
/** Install a profile (start-up only; the strings must outlive every view). */
void set_platform_profile(const PlatformProfile &profile);

/** The Settings rows the profile shows (the T-Deck's seven without a callback). */
size_t platform_settings_rows(SettingsRowId *out, size_t capacity);
/** Whether the System Menu lists Difficulty and Cheats now. */
bool platform_enhanced_rows();
/** Whether Cast and Mix take a typed spell name now (false on the T-Deck). */
bool platform_typed_spell_names();
/** Whether Q is (Q)uit & Save now (false on the T-Deck). */
bool platform_quit_save_key();
/** Whether the 1988 interaction model is active now (false on the T-Deck). */
bool platform_dos_interface();

/**
 * The DATA.OVL tables the 1988 list panels print, by their DGROUP address
 * (re/notes/use-picker-panel.md, ready-picker-panel.md, ztats-layout.md).
 * `index` is the table's own index; a Literal is addressed by its DS offset.
 */
enum class DosText : uint8_t {
    EquipmentShort, // DS 0x1962: 48 short equipment names ("Leath Helm"), the Ready picker / Arms
    EquipmentGlyph, // DS 0x1ae8: 48 RUNES.CH class glyphs, one byte as a 1-char string
    UseItem,        // DS 0x1916: 38 (U)se entries, with their '*' '!' '(' sigils
    PotionColour,   // DS 0x19c2: 8 potion colours, the '!' entries' side strings
    Reagent,        // DS 0x19d2: 8 short reagent names ("Blk. Pearl")
    Spell,          // DS 0x19e2: 48 short spell names ("In Sanct G")
    Literal,        // a NUL-terminated DGROUP string at DS `index` ("--", "Moonstone ")
};
/** The platform's DOS text, or nullptr (always on the T-Deck). */
const char *platform_dos_text(DosText table, int index);
/**
 * Copies `text` into `out` (always terminated, truncated to fit), replacing
 * {back}, {move}, {menu}, {import}, {export} and {storage} with the
 * profile's words.
 * With the default profile every result is the T-Deck's literal text.
 */
size_t platform_format(char *out, size_t capacity, const char *text);

} // namespace openu5
