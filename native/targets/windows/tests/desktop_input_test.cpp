// Ultima V Native for Windows -- uv::DesktopInput::translate, table by table.
//
// Every event is a hand-built SDL_Event (no SDL_Init, no window): key-downs
// with a scancode, modifier mask and repeat flag, and SDL_EVENT_TEXT_INPUT
// with a UTF-8 string. The expectations are the key map documented in
// src/app/desktop_input.h: both interface modes share the DOS keyboard
// (arrows, Enter, Esc, Backspace, PgUp/PgDn, digits in "Player:" prompts,
// F10 / Alt+M, the mute keys, the window keys); Enhanced adds F5/F9/Alt+S/
// Alt+L/F2, WASD Movement Mode and Alt+D (which Original grants only with the
// developer switch).
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

#include "app/desktop_input.h"
#include "test_check.h"

using openu5::Direction;
using openu5::UiActionKind;
using openu5::UiMode;
using openu5::UiRequestId;
using tdeck::DeviceShortcut;
using uv::DesktopCommand;
using uv::DesktopInput;
using uv::InputContext;
using uv::InterfaceMode;
using uvtest::check;
using uvtest::format;

namespace {

constexpr SDL_Keymod kNone = 0;
constexpr SDL_Keymod kShift = SDL_KMOD_LSHIFT;
constexpr SDL_Keymod kCtrl = SDL_KMOD_LCTRL;
constexpr SDL_Keymod kAlt = SDL_KMOD_LALT;
constexpr SDL_Keymod kRAlt = SDL_KMOD_RALT;
constexpr SDL_Keymod kNum = SDL_KMOD_NUM;
// What SDL3 on Windows reports for AltGr: Right Alt alone (it drops the fake
// left Ctrl that Windows sends with it). The older Ctrl+Right Alt form is kept
// as kAltGrLegacy, for when SDL's heuristic misses the fake Ctrl.
constexpr SDL_Keymod kAltGr = SDL_KMOD_RALT;
constexpr SDL_Keymod kAltGrLegacy = SDL_KMOD_LCTRL | SDL_KMOD_RALT;

SDL_Event key(SDL_Scancode sc, SDL_Keymod mod = kNone, bool repeat = false) {
    SDL_Event e;
    std::memset(&e, 0, sizeof e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.type = SDL_EVENT_KEY_DOWN;
    e.key.scancode = sc;
    e.key.mod = mod;
    e.key.down = true;
    e.key.repeat = repeat;
    return e;
}

SDL_Event text(const char *utf8) {
    SDL_Event e;
    std::memset(&e, 0, sizeof e);
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.type = SDL_EVENT_TEXT_INPUT;
    e.text.text = utf8;
    return e;
}

SDL_Event plain(SDL_EventType type) {
    SDL_Event e;
    std::memset(&e, 0, sizeof e);
    e.type = type;
    return e;
}

struct Out {
    size_t n = 0;
    DesktopCommand c[DesktopInput::kMaxCommands]{};
};

Out run(DesktopInput &in, const SDL_Event &e, const InputContext &ctx = {}) {
    Out o;
    o.n = in.translate(e, ctx, o.c, DesktopInput::kMaxCommands);
    return o;
}

const char *kind_name(UiActionKind k) {
    switch (k) {
    case UiActionKind::Direction: return "Direction";
    case UiActionKind::Character: return "Character";
    case UiActionKind::Confirm: return "Confirm";
    case UiActionKind::Cancel: return "Cancel";
    case UiActionKind::Back: return "Back";
    case UiActionKind::Next: return "Next";
    case UiActionKind::Previous: return "Previous";
    case UiActionKind::PageUp: return "PageUp";
    case UiActionKind::PageDown: return "PageDown";
    case UiActionKind::SelectIndex: return "SelectIndex";
    case UiActionKind::TextInput: return "TextInput";
    case UiActionKind::DeleteCharacter: return "DeleteCharacter";
    case UiActionKind::SystemMenu: return "SystemMenu";
    case UiActionKind::HighlightIndex: return "HighlightIndex";
    }
    return "?";
}

const char *shortcut_name(DeviceShortcut s) {
    switch (s) {
    case DeviceShortcut::None: return "None";
    case DeviceShortcut::DeveloperMenu: return "DeveloperMenu";
    case DeviceShortcut::Save: return "Save";
    case DeviceShortcut::Load: return "Load";
    case DeviceShortcut::MovementModeToggled: return "MovementModeToggled";
    case DeviceShortcut::MusicMute: return "MusicMute";
    case DeviceShortcut::SfxMute: return "SfxMute";
    }
    return "?";
}

const char *dir_name(Direction d) {
    switch (d) {
    case Direction::North: return "North";
    case Direction::South: return "South";
    case Direction::East: return "East";
    case Direction::West: return "West";
    }
    return "?";
}

std::string describe(const Out &o) {
    std::string s = format("%zu command(s)", o.n);
    for (size_t i = 0; i < o.n && i < DesktopInput::kMaxCommands; ++i) {
        const DesktopCommand &c = o.c[i];
        switch (c.kind) {
        case DesktopCommand::Kind::None: s += " [None]"; break;
        case DesktopCommand::Kind::ToggleFullscreen: s += " [ToggleFullscreen]"; break;
        case DesktopCommand::Kind::WindowScaleUp: s += " [WindowScaleUp]"; break;
        case DesktopCommand::Kind::WindowScaleDown: s += " [WindowScaleDown]"; break;
        case DesktopCommand::Kind::Screenshot: s += " [Screenshot]"; break;
        case DesktopCommand::Kind::Game:
            s += format(" [Game %s dir=%s char=0x%02x index=%d shortcut=%s source=%s]", kind_name(c.action.kind),
                        dir_name(c.action.direction), unsigned(c.action.character), int(c.action.index),
                        shortcut_name(c.shortcut), c.source ? c.source : "(null)");
            break;
        }
    }
    return s;
}

// ---- expectations ------------------------------------------------------------
bool is_game(const Out &o, UiActionKind k) {
    return o.n == 1 && o.c[0].kind == DesktopCommand::Kind::Game && o.c[0].action.kind == k &&
           o.c[0].shortcut == DeviceShortcut::None;
}
bool is_dir(const Out &o, Direction d) { return is_game(o, UiActionKind::Direction) && o.c[0].action.direction == d; }
bool is_char(const Out &o, char ch) { return is_game(o, UiActionKind::Character) && o.c[0].action.character == char16_t(ch); }
bool is_highlight(const Out &o, int index) {
    return is_game(o, UiActionKind::HighlightIndex) && o.c[0].action.index == index;
}
bool is_shortcut(const Out &o, DeviceShortcut s) {
    // A device shortcut carries no key: its action must NOT be UiAction's
    // default kind (Confirm), or a screen would read it as Enter.
    return o.n == 1 && o.c[0].kind == DesktopCommand::Kind::Game && o.c[0].shortcut == s &&
           o.c[0].action.kind == UiActionKind::Character && o.c[0].action.character == 0;
}
bool is_window(const Out &o, DesktopCommand::Kind k) { return o.n == 1 && o.c[0].kind == k; }
bool none(const Out &o) { return o.n == 0; }

InputContext ctx(UiMode mode, UiRequestId request = UiRequestId::None, bool accepts_direction = false,
                 bool movement_mode = false, bool developer_tools = false) {
    InputContext c;
    c.mode = mode;
    c.request = request;
    c.accepts_direction = accepts_direction;
    c.movement_mode = movement_mode;
    c.developer_tools = developer_tools;
    return c;
}

const InterfaceMode kModes[] = {InterfaceMode::Original, InterfaceMode::Enhanced};
const char *mode_tag(InterfaceMode m) { return m == InterfaceMode::Enhanced ? "Enhanced" : "Original"; }

DesktopInput input_in(InterfaceMode m) {
    DesktopInput in;
    in.set_mode(m);
    return in;
}

// ---- tables ------------------------------------------------------------------

void directions_and_paging() {
    struct Row {
        const char *name;
        SDL_Scancode sc;
        SDL_Keymod mod;
        Direction dir;
    };
    const Row rows[] = {
        {"Up", SDL_SCANCODE_UP, kNone, Direction::North},
        {"Down", SDL_SCANCODE_DOWN, kNone, Direction::South},
        {"Left", SDL_SCANCODE_LEFT, kNone, Direction::West},
        {"Right", SDL_SCANCODE_RIGHT, kNone, Direction::East},
        {"KP8 NumLock off", SDL_SCANCODE_KP_8, kNone, Direction::North},
        {"KP2 NumLock off", SDL_SCANCODE_KP_2, kNone, Direction::South},
        {"KP4 NumLock off", SDL_SCANCODE_KP_4, kNone, Direction::West},
        {"KP6 NumLock off", SDL_SCANCODE_KP_6, kNone, Direction::East},
        {"Shift+Left", SDL_SCANCODE_LEFT, kShift, Direction::West},
        {"Shift+Right", SDL_SCANCODE_RIGHT, kShift, Direction::East},
    };
    for (InterfaceMode m : kModes) {
        for (const Row &r : rows) {
            DesktopInput in = input_in(m);
            const Out o = run(in, key(r.sc, r.mod));
            check(is_dir(o, r.dir), format("D1 %s: %s -> Direction %s", mode_tag(m), r.name, dir_name(r.dir)), describe(o));
            // OS auto-repeat is allowed for movement.
            const Out rep = run(in, key(r.sc, r.mod, true));
            check(is_dir(rep, r.dir), format("D2 %s: %s held (OS repeat) -> Direction %s again", mode_tag(m), r.name,
                                             dir_name(r.dir)),
                  describe(rep));
        }
        // NumLock ON: the numpad types digits (SDL then sends the digit as text).
        const SDL_Scancode pads[] = {SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6};
        const char *digits[] = {"8", "2", "4", "6"};
        for (int i = 0; i < 4; ++i) {
            DesktopInput in = input_in(m);
            const Out down = run(in, key(pads[i], kNum));
            const Out typed = run(in, text(digits[i]));
            check(none(down) && is_char(typed, digits[i][0]),
                  format("D3 %s: KP%s with NumLock on is no direction; its text '%s' is a Character", mode_tag(m), digits[i],
                         digits[i]),
                  describe(down) + " / " + describe(typed));
        }
        {
            // NumLock OFF: the direction is taken on key-down and any text is suppressed.
            DesktopInput in = input_in(m);
            run(in, key(SDL_SCANCODE_KP_8, kNone));
            const Out typed = run(in, text("8"));
            check(none(typed), format("D4 %s: a numpad direction (NumLock off) suppresses its text", mode_tag(m)), describe(typed));
        }
        // Shift+Up/Down page the console (the T-Deck's Shift + trackball).
        {
            DesktopInput in = input_in(m);
            const Out up = run(in, key(SDL_SCANCODE_UP, kShift));
            const Out down = run(in, key(SDL_SCANCODE_DOWN, SDL_KMOD_RSHIFT));
            const Out kp = run(in, key(SDL_SCANCODE_KP_8, kShift));
            const Out up_rep = run(in, key(SDL_SCANCODE_UP, kShift, true));
            check(is_game(up, UiActionKind::PageUp), format("D5 %s: Shift+Up -> PageUp", mode_tag(m)), describe(up));
            check(is_game(down, UiActionKind::PageDown), format("D5 %s: RShift+Down -> PageDown", mode_tag(m)), describe(down));
            check(is_game(kp, UiActionKind::PageUp), format("D5 %s: Shift+KP8 (NumLock off) -> PageUp", mode_tag(m)), describe(kp));
            check(is_game(up_rep, UiActionKind::PageUp), format("D5 %s: Shift+Up held -> PageUp repeats", mode_tag(m)),
                  describe(up_rep));
        }
        {
            DesktopInput in = input_in(m);
            const Out pu = run(in, key(SDL_SCANCODE_PAGEUP));
            const Out pd = run(in, key(SDL_SCANCODE_PAGEDOWN));
            const Out pu_rep = run(in, key(SDL_SCANCODE_PAGEUP, kNone, true));
            const Out pd_rep = run(in, key(SDL_SCANCODE_PAGEDOWN, kNone, true));
            check(is_game(pu, UiActionKind::PageUp) && is_game(pd, UiActionKind::PageDown),
                  format("D6 %s: PgUp/PgDn -> PageUp/PageDown", mode_tag(m)), describe(pu) + " / " + describe(pd));
            check(is_game(pu_rep, UiActionKind::PageUp) && is_game(pd_rep, UiActionKind::PageDown),
                  format("D6 %s: PgUp/PgDn held (OS repeat) keep paging", mode_tag(m)),
                  describe(pu_rep) + " / " + describe(pd_rep));
        }
        {
            // A Ctrl or Alt chord on an arrow is no movement.
            DesktopInput in = input_in(m);
            const Out c = run(in, key(SDL_SCANCODE_UP, kCtrl));
            const Out a = run(in, key(SDL_SCANCODE_LEFT, kAlt));
            check(none(c) && none(a), format("D7 %s: Ctrl+Up and Alt+Left are not directions", mode_tag(m)),
                  describe(c) + " / " + describe(a));
        }
    }
}

void confirm_cancel_back() {
    for (InterfaceMode m : kModes) {
        DesktopInput in = input_in(m);
        const Out enter = run(in, key(SDL_SCANCODE_RETURN));
        const Out kp_enter = run(in, key(SDL_SCANCODE_KP_ENTER));
        const Out esc = run(in, key(SDL_SCANCODE_ESCAPE));
        check(is_game(enter, UiActionKind::Confirm), format("C1 %s: Enter -> Confirm", mode_tag(m)), describe(enter));
        check(is_game(kp_enter, UiActionKind::Confirm), format("C1 %s: keypad Enter -> Confirm", mode_tag(m)), describe(kp_enter));
        check(is_game(esc, UiActionKind::Cancel), format("C2 %s: Escape -> Cancel", mode_tag(m)), describe(esc));
        const Out enter_rep = run(in, key(SDL_SCANCODE_RETURN, kNone, true));
        const Out esc_rep = run(in, key(SDL_SCANCODE_ESCAPE, kNone, true));
        check(none(enter_rep) && none(esc_rep), format("C3 %s: held Enter / Escape do not repeat", mode_tag(m)),
              describe(enter_rep) + " / " + describe(esc_rep));

        struct Row {
            UiMode mode;
            const char *name;
            UiActionKind want;
        };
        const Row rows[] = {
            {UiMode::TextEntry, "TextEntry", UiActionKind::DeleteCharacter},
            {UiMode::NumericEntry, "NumericEntry", UiActionKind::DeleteCharacter},
            {UiMode::Exploration, "Exploration", UiActionKind::Back},
            {UiMode::Dialogue, "Dialogue", UiActionKind::Back},
            {UiMode::YesNo, "YesNo", UiActionKind::Back},
            {UiMode::PartySelection, "PartySelection", UiActionKind::Back},
            {UiMode::Shop, "Shop", UiActionKind::Back},
            {UiMode::Combat, "Combat", UiActionKind::Back},
        };
        for (const Row &r : rows) {
            DesktopInput b = input_in(m);
            const Out o = run(b, key(SDL_SCANCODE_BACKSPACE), ctx(r.mode));
            const Out rep = run(b, key(SDL_SCANCODE_BACKSPACE, kNone, true), ctx(r.mode));
            check(is_game(o, r.want), format("C4 %s: Backspace in %s -> %s", mode_tag(m), r.name, kind_name(r.want)), describe(o));
            check(is_game(rep, r.want), format("C5 %s: Backspace held in %s repeats (%s)", mode_tag(m), r.name, kind_name(r.want)),
                  describe(rep));
        }
        {
            // Backspace's own text (0x08) never reaches the game as a Character.
            DesktopInput b = input_in(m);
            run(b, key(SDL_SCANCODE_BACKSPACE), ctx(UiMode::TextEntry));
            const Out t = run(b, text("\b"), ctx(UiMode::TextEntry));
            check(none(t), format("C6 %s: Backspace's text is suppressed", mode_tag(m)), describe(t));
        }
    }
}

void repeat_policy() {
    for (InterfaceMode m : kModes) {
        {
            DesktopInput in = input_in(m);
            const Out down = run(in, key(SDL_SCANCODE_A));
            const Out t = run(in, text("a"));
            check(none(down) && is_char(t, 'a'), format("R1 %s: a letter key arrives as its text: 'a'", mode_tag(m)),
                  describe(down) + " / " + describe(t));
            const Out rep = run(in, key(SDL_SCANCODE_A, kNone, true));
            const Out t2 = run(in, text("a"));
            check(none(rep) && none(t2), format("R2 %s: a held letter (OS repeat) acts once: the repeat's text is dropped", mode_tag(m)),
                  describe(rep) + " / " + describe(t2));
            const Out again = run(in, key(SDL_SCANCODE_A));
            const Out t3 = run(in, text("a"));
            check(is_char(t3, 'a') && none(again), format("R3 %s: the next real press types again", mode_tag(m)), describe(t3));
        }
        struct Row {
            const char *name;
            SDL_Scancode sc;
            SDL_Keymod mod;
        };
        const Row once[] = {
            {"F10", SDL_SCANCODE_F10, kNone},       {"F7", SDL_SCANCODE_F7, kNone},
            {"F8", SDL_SCANCODE_F8, kNone},         {"F11", SDL_SCANCODE_F11, kNone},
            {"F12", SDL_SCANCODE_F12, kNone},       {"Alt+Enter", SDL_SCANCODE_RETURN, kAlt},
            {"Ctrl+=", SDL_SCANCODE_EQUALS, kCtrl}, {"Ctrl+-", SDL_SCANCODE_MINUS, kCtrl},
            {"Alt+M", SDL_SCANCODE_M, kAlt},        {"Ctrl+S", SDL_SCANCODE_S, kCtrl},
            {"F5", SDL_SCANCODE_F5, kNone},         {"F9", SDL_SCANCODE_F9, kNone},
            {"F2", SDL_SCANCODE_F2, kNone},         {"Alt+D", SDL_SCANCODE_D, kAlt},
        };
        for (const Row &r : once) {
            DesktopInput in = input_in(m);
            const Out o = run(in, key(r.sc, r.mod, true), ctx(UiMode::Exploration, UiRequestId::None, false, false, true));
            check(none(o), format("R4 %s: %s held (OS repeat) does nothing", mode_tag(m), r.name), describe(o));
        }
    }
}

void text_input() {
    for (InterfaceMode m : kModes) {
        struct Row {
            const char *utf8;
            const char *name;
            std::vector<char> want;
        };
        const Row rows[] = {
            {"a", "'a'", {'a'}},
            {"Z", "'Z'", {'Z'}},
            {" ", "space", {' '}},
            {"~", "'~'", {'~'}},
            {"?", "'?'", {'?'}},
            {"0", "'0'", {'0'}},
            {"\xc3\xa9", "U+00E9 (two UTF-8 bytes)", {}},
            {"\xe2\x82\xac", "U+20AC euro (three bytes)", {}},
            {"\xf0\x9f\x98\x80", "U+1F600 (four bytes)", {}},
            {"a\xc3\xa9" "1", "'a' + U+00E9 + '1'", {'a', '1'}},
            {"\x7f", "DEL", {}},
            {"\t", "TAB", {}},
            {"\x1b", "ESC byte", {}},
            {"", "empty text", {}},
        };
        for (const Row &r : rows) {
            DesktopInput in = input_in(m);
            const Out o = run(in, text(r.utf8), ctx(UiMode::Dialogue));
            bool good = o.n == r.want.size();
            for (size_t i = 0; good && i < o.n; ++i)
                good = o.c[i].kind == DesktopCommand::Kind::Game && o.c[i].action.kind == UiActionKind::Character &&
                       o.c[i].action.character == char16_t(r.want[i]) && o.c[i].shortcut == DeviceShortcut::None;
            check(good, format("T1 %s: text %s -> %zu Character(s), non-ASCII dropped", mode_tag(m), r.name, r.want.size()),
                  describe(o));
        }
        {
            DesktopInput in = input_in(m);
            Out o;
            o.n = in.translate(text("abcdef"), ctx(UiMode::Dialogue), o.c, DesktopInput::kMaxCommands);
            check(o.n == DesktopInput::kMaxCommands && o.c[0].action.character == u'a' && o.c[3].action.character == u'd',
                  format("T2 %s: a 6-character text fills at most kMaxCommands (%zu) commands, in order", mode_tag(m),
                         DesktopInput::kMaxCommands),
                  describe(o));
            DesktopCommand one[1];
            const size_t n1 = in.translate(text("xy"), ctx(UiMode::Dialogue), one, 1);
            check(n1 == 1 && one[0].action.character == u'x', format("T3 %s: capacity 1 keeps the first character", mode_tag(m)),
                  format("n=%zu", n1));
            check(in.translate(text("x"), ctx(UiMode::Dialogue), one, 0) == 0 &&
                      in.translate(text("x"), ctx(UiMode::Dialogue), nullptr, 4) == 0,
                  format("T4 %s: no output buffer (capacity 0 or null) -> 0 commands", mode_tag(m)));
        }
        {
            DesktopInput in = input_in(m);
            const Out t = run(in, text(nullptr), ctx(UiMode::Dialogue));
            check(none(t), format("T5 %s: a null text pointer is ignored", mode_tag(m)), describe(t));
        }
    }
}

void party_selection() {
    for (InterfaceMode m : kModes) {
        // Kernel 0x2d7a: a digit 1-6 only moves the highlight; RETURN or Space chooses.
        for (char d = '1'; d <= '6'; ++d) {
            DesktopInput in = input_in(m);
            const char s[2] = {d, 0};
            const Out o = run(in, text(s), ctx(UiMode::PartySelection, UiRequestId::Party));
            check(is_highlight(o, d - '1'),
                  format("P1 %s: '%c' in a Player: prompt -> HighlightIndex(%d), not a choice", mode_tag(m), d, d - '1'),
                  describe(o));
        }
        for (char d = '7'; d <= '9'; ++d) {
            DesktopInput in = input_in(m);
            const char s[2] = {d, 0};
            const Out o = run(in, text(s), ctx(UiMode::PartySelection, UiRequestId::CastMember));
            check(is_char(o, d), format("P1 %s: '%c' (no seventh member) stays a Character, which the picker drops", mode_tag(m), d),
                  describe(o));
        }
        {
            DesktopInput in = input_in(m);
            const Out space = run(in, text(" "), ctx(UiMode::PartySelection, UiRequestId::CastMember));
            const Out camp = run(in, text(" "), ctx(UiMode::PartySelection, UiRequestId::CampGuard));
            const Out mix = run(in, text(" "), ctx(UiMode::PartySelection, UiRequestId::MixReagents));
            const Out outside = run(in, text(" "), ctx(UiMode::Exploration));
            check(is_game(space, UiActionKind::Confirm), format("P6 %s: Space in a Player: prompt chooses (Confirm)", mode_tag(m)),
                  describe(space));
            check(is_char(mix, ' ') && is_char(outside, ' '),
                  format("P6 %s: Space stays a Character for the Mix picker and outside prompts", mode_tag(m)),
                  describe(mix) + " / " + describe(outside));
            // Original: the Camp guard is the same roster picker (kernel 0x3eac
            // -> 0x2e8e -> 0x2d7a), so Space chooses there too; Enhanced keeps
            // the watch's own keys.
            check(m == InterfaceMode::Original ? is_game(camp, UiActionKind::Confirm) : is_char(camp, ' '),
                  format("P6 %s: Space for the Camp watch is %s", mode_tag(m),
                         m == InterfaceMode::Original ? "Confirm (0x2d7a)" : "a Character (its own keys)"),
                  describe(camp));
        }
        {
            DesktopInput in = input_in(m);
            const Out zero = run(in, text("0"), ctx(UiMode::PartySelection, UiRequestId::Party));
            const Out letter = run(in, text("q"), ctx(UiMode::PartySelection, UiRequestId::Party));
            check(is_char(zero, '0') && is_char(letter, 'q'),
                  format("P2 %s: '0' and letters in a Player: prompt stay Characters", mode_tag(m)),
                  describe(zero) + " / " + describe(letter));
        }
        const UiRequestId own_digits[] = {UiRequestId::CampGuard, UiRequestId::MixReagents};
        const char *own_names[] = {"CampGuard", "MixReagents"};
        for (int i = 0; i < 2; ++i) {
            for (char d : {'1', '5', '9'}) {
                DesktopInput in = input_in(m);
                const char s[2] = {d, 0};
                const Out o = run(in, text(s), ctx(UiMode::PartySelection, own_digits[i]));
                // Original's Camp guard highlights like every 0x2d7a prompt.
                if (m == InterfaceMode::Original && own_digits[i] == UiRequestId::CampGuard && d != '9')
                    check(is_highlight(o, d - '1'),
                          format("P3 %s: '%c' with request CampGuard -> HighlightIndex(%d) (0x2d7a)", mode_tag(m), d, d - '1'),
                          describe(o));
                else
                    check(is_char(o, d), format("P3 %s: '%c' with request %s stays a Character (the picker reads digits itself)",
                                                mode_tag(m), d, own_names[i]),
                          describe(o));
            }
        }
        {
            DesktopInput in = input_in(m);
            const Out a = run(in, text("3"), ctx(UiMode::PartySelection, UiRequestId::None));
            const Out b = run(in, text("3"), ctx(UiMode::PartySelection, UiRequestId::SearchMember));
            const Out c = run(in, text("3"), ctx(UiMode::Exploration));
            const Out e = run(in, text("3"), ctx(UiMode::NumericEntry));
            check(is_highlight(a, 2) && is_highlight(b, 2),
                  format("P4 %s: other Player: requests highlight the member number", mode_tag(m)),
                  describe(a) + " / " + describe(b));
            check(is_char(c, '3') && is_char(e, '3'), format("P5 %s: outside PartySelection a digit is a Character", mode_tag(m)),
                  describe(c) + " / " + describe(e));
        }
    }
}

void system_mute_window() {
    for (InterfaceMode m : kModes) {
        struct Row {
            const char *name;
            SDL_Scancode sc;
            SDL_Keymod mod;
            DeviceShortcut want;
            const char *source;
        };
        const Row mutes[] = {
            {"F7", SDL_SCANCODE_F7, kNone, DeviceShortcut::MusicMute, "f7"},
            {"F8", SDL_SCANCODE_F8, kNone, DeviceShortcut::SfxMute, "f8"},
            {"Ctrl+M", SDL_SCANCODE_M, kCtrl, DeviceShortcut::MusicMute, "ctrl-m"},
            {"Ctrl+S", SDL_SCANCODE_S, kCtrl, DeviceShortcut::SfxMute, "ctrl-s"},
            {"RCtrl+S", SDL_SCANCODE_S, SDL_KMOD_RCTRL, DeviceShortcut::SfxMute, "ctrl-s"},
            {"Alt+Shift+M", SDL_SCANCODE_M, SDL_Keymod(kAlt | kShift), DeviceShortcut::MusicMute, "alt-shift-m"},
            {"Alt+Shift+S", SDL_SCANCODE_S, SDL_Keymod(kAlt | kShift), DeviceShortcut::SfxMute, "alt-shift-s"},
        };
        for (const Row &r : mutes) {
            DesktopInput in = input_in(m);
            const Out o = run(in, key(r.sc, r.mod));
            check(is_shortcut(o, r.want) && std::strcmp(o.c[0].source, r.source) == 0,
                  format("S1 %s: %s -> %s (source \"%s\"), action kind not the default Confirm", mode_tag(m), r.name,
                         shortcut_name(r.want), r.source),
                  describe(o));
        }
        {
            DesktopInput in = input_in(m);
            const Out f10 = run(in, key(SDL_SCANCODE_F10));
            const Out altm = run(in, key(SDL_SCANCODE_M, kAlt));
            const Out raltm = run(in, key(SDL_SCANCODE_M, kRAlt));
            check(is_game(f10, UiActionKind::SystemMenu) && std::strcmp(f10.c[0].source, "f10") == 0,
                  format("S2 %s: F10 -> SystemMenu", mode_tag(m)), describe(f10));
            check(is_game(altm, UiActionKind::SystemMenu) && std::strcmp(altm.c[0].source, "alt-m") == 0,
                  format("S2 %s: Alt+M -> SystemMenu", mode_tag(m)), describe(altm));
            check(is_game(raltm, UiActionKind::SystemMenu), format("S2 %s: Right Alt (no Ctrl) + M is Alt+M -> SystemMenu", mode_tag(m)),
                  describe(raltm));
            const Out ctrl_f10 = run(in, key(SDL_SCANCODE_F10, kCtrl));
            const Out alt_f10 = run(in, key(SDL_SCANCODE_F10, kAlt));
            check(none(ctrl_f10) && none(alt_f10), format("S3 %s: Ctrl+F10 / Alt+F10 are not the System Menu", mode_tag(m)),
                  describe(ctrl_f10) + " / " + describe(alt_f10));
        }
        struct WinRow {
            const char *name;
            SDL_Scancode sc;
            SDL_Keymod mod;
            DesktopCommand::Kind want;
        };
        const WinRow windows[] = {
            {"Alt+Enter", SDL_SCANCODE_RETURN, kAlt, DesktopCommand::Kind::ToggleFullscreen},
            {"Alt+KP_Enter", SDL_SCANCODE_KP_ENTER, kAlt, DesktopCommand::Kind::ToggleFullscreen},
            {"F11", SDL_SCANCODE_F11, kNone, DesktopCommand::Kind::ToggleFullscreen},
            {"Ctrl+=", SDL_SCANCODE_EQUALS, kCtrl, DesktopCommand::Kind::WindowScaleUp},
            {"Ctrl+KP_Plus", SDL_SCANCODE_KP_PLUS, kCtrl, DesktopCommand::Kind::WindowScaleUp},
            {"Ctrl+-", SDL_SCANCODE_MINUS, kCtrl, DesktopCommand::Kind::WindowScaleDown},
            {"Ctrl+KP_Minus", SDL_SCANCODE_KP_MINUS, kCtrl, DesktopCommand::Kind::WindowScaleDown},
            {"F12", SDL_SCANCODE_F12, kNone, DesktopCommand::Kind::Screenshot},
        };
        for (const WinRow &r : windows) {
            DesktopInput in = input_in(m);
            const Out o = run(in, key(r.sc, r.mod));
            check(is_window(o, r.want) && o.c[0].shortcut == DeviceShortcut::None,
                  format("W1 %s: %s -> window command (not a game key)", mode_tag(m), r.name), describe(o));
        }
        {
            // Without Ctrl, '=' and '-' are the game's own keys (their text).
            DesktopInput in = input_in(m);
            const Out k = run(in, key(SDL_SCANCODE_EQUALS));
            const Out t = run(in, text("="));
            check(none(k) && is_char(t, '='), format("W2 %s: '=' without Ctrl is a Character", mode_tag(m)),
                  describe(k) + " / " + describe(t));
        }
    }
}

void original_vs_enhanced() {
    struct Row {
        const char *name;
        SDL_Scancode sc;
        SDL_Keymod mod;
        DeviceShortcut enhanced;
    };
    const Row rows[] = {
        {"F5", SDL_SCANCODE_F5, kNone, DeviceShortcut::Save},
        {"F9", SDL_SCANCODE_F9, kNone, DeviceShortcut::Load},
        {"Alt+S", SDL_SCANCODE_S, kAlt, DeviceShortcut::Save},
        {"Alt+L", SDL_SCANCODE_L, kAlt, DeviceShortcut::Load},
        {"F2", SDL_SCANCODE_F2, kNone, DeviceShortcut::MovementModeToggled},
        {"Alt+D", SDL_SCANCODE_D, kAlt, DeviceShortcut::DeveloperMenu},
    };
    for (const Row &r : rows) {
        DesktopInput e = input_in(InterfaceMode::Enhanced);
        DesktopInput o = input_in(InterfaceMode::Original);
        const Out eo = run(e, key(r.sc, r.mod));
        const Out oo = run(o, key(r.sc, r.mod));
        check(is_shortcut(eo, r.enhanced), format("E1 Enhanced: %s -> %s", r.name, shortcut_name(r.enhanced)), describe(eo));
        check(none(oo), format("E2 Original: %s does nothing", r.name), describe(oo));
        // Whatever the chord did, its letter must not type afterwards.
        if (r.mod & kAlt) {
            const char letter[2] = {char('a' + (r.sc - SDL_SCANCODE_A)), 0};
            const Out t = run(o, text(letter));
            check(none(t), format("E3 Original: the text after %s is suppressed", r.name), describe(t));
        }
    }
    {
        DesktopInput o = input_in(InterfaceMode::Original);
        DesktopInput e = input_in(InterfaceMode::Enhanced);
        const InputContext dev = ctx(UiMode::Exploration, UiRequestId::None, false, false, true);
        const Out od = run(o, key(SDL_SCANCODE_D, kAlt), dev);
        const Out ed = run(e, key(SDL_SCANCODE_D, kAlt), dev);
        check(is_shortcut(od, DeviceShortcut::DeveloperMenu), "E4 Original + developer_tools: Alt+D -> DeveloperMenu", describe(od));
        check(is_shortcut(ed, DeviceShortcut::DeveloperMenu), "E4 Enhanced + developer_tools: Alt+D -> DeveloperMenu", describe(ed));
        const Out of5 = run(o, key(SDL_SCANCODE_F5), dev);
        const Out ef5alt = run(e, key(SDL_SCANCODE_F5, kAlt));
        check(none(of5), "E5 Original + developer_tools: F5 still does nothing (developer_tools grants Alt+D only)", describe(of5));
        check(none(ef5alt), "E5 Enhanced: Alt+F5 is not quick save", describe(ef5alt));
    }
    // WASD Movement Mode.
    struct Wasd {
        char ch;
        Direction dir;
    };
    const Wasd wasd[] = {{'w', Direction::North}, {'a', Direction::West}, {'s', Direction::South}, {'d', Direction::East},
                         {'W', Direction::North}, {'A', Direction::West}, {'S', Direction::South}, {'D', Direction::East}};
    struct Ctx {
        const char *name;
        InputContext c;
    };
    const Ctx movement[] = {
        {"Exploration", ctx(UiMode::Exploration, UiRequestId::None, false, true)},
        {"Dungeon", ctx(UiMode::Dungeon, UiRequestId::None, false, true)},
        {"Combat", ctx(UiMode::Combat, UiRequestId::None, false, true)},
        {"TargetSelection", ctx(UiMode::TargetSelection, UiRequestId::Target, false, true)},
        {"YesNo accepting a direction", ctx(UiMode::YesNo, UiRequestId::Direction, true, true)},
    };
    for (const Ctx &c : movement) {
        for (const Wasd &w : wasd) {
            DesktopInput e = input_in(InterfaceMode::Enhanced);
            DesktopInput o = input_in(InterfaceMode::Original);
            const char s[2] = {w.ch, 0};
            const Out eo = run(e, text(s), c.c);
            const Out oo = run(o, text(s), c.c);
            check(is_dir(eo, w.dir) && std::strcmp(eo.c[0].source, "wasd") == 0,
                  format("M1 Enhanced, Movement Mode on, %s: '%c' -> Direction %s", c.name, w.ch, dir_name(w.dir)), describe(eo));
            check(is_char(oo, w.ch), format("M2 Original (movement_mode set), %s: '%c' stays a Character", c.name, w.ch),
                  describe(oo));
        }
        DesktopInput e = input_in(InterfaceMode::Enhanced);
        const Out x = run(e, text("x"), c.c);
        check(is_char(x, 'x'), format("M3 Enhanced, Movement Mode on, %s: other letters stay Characters", c.name), describe(x));
    }
    const Ctx still[] = {
        {"TextEntry", ctx(UiMode::TextEntry, UiRequestId::YellText, false, true)},
        {"Dialogue", ctx(UiMode::Dialogue, UiRequestId::Dialogue, false, true)},
        {"Shop", ctx(UiMode::Shop, UiRequestId::Shop, false, true)},
        {"YesNo", ctx(UiMode::YesNo, UiRequestId::None, false, true)},
    };
    for (const Ctx &c : still) {
        DesktopInput e = input_in(InterfaceMode::Enhanced);
        const Out o = run(e, text("w"), c.c);
        check(is_char(o, 'w'), format("M4 Enhanced, Movement Mode on, %s (no movement context): 'w' is a Character", c.name),
              describe(o));
    }
    {
        DesktopInput e = input_in(InterfaceMode::Enhanced);
        const Out o = run(e, text("w"), ctx(UiMode::Exploration, UiRequestId::None, false, false));
        check(is_char(o, 'w'), "M5 Enhanced, Movement Mode OFF: 'w' is a Character", describe(o));
        const Out multi = run(e, text("wxd"), ctx(UiMode::Exploration, UiRequestId::None, false, true));
        check(multi.n == 3 && multi.c[0].action.kind == UiActionKind::Direction && multi.c[0].action.direction == Direction::North &&
                  multi.c[1].action.kind == UiActionKind::Character && multi.c[1].action.character == u'x' &&
                  multi.c[2].action.kind == UiActionKind::Direction && multi.c[2].action.direction == Direction::East,
              "M6 Enhanced, Movement Mode on: \"wxd\" -> North, 'x', East", describe(multi));
    }
}

void chords_and_altgr() {
    for (InterfaceMode m : kModes) {
        struct Row {
            const char *name;
            SDL_Scancode sc;
            SDL_Keymod mod;
            const char *typed;
        };
        const Row chords[] = {
            {"Alt+M", SDL_SCANCODE_M, kAlt, "m"},
            {"Alt+X (unmapped)", SDL_SCANCODE_X, kAlt, "x"},
            {"Ctrl+S", SDL_SCANCODE_S, kCtrl, "s"},
            {"Ctrl+Q (unmapped)", SDL_SCANCODE_Q, kCtrl, "q"},
            {"Ctrl+=", SDL_SCANCODE_EQUALS, kCtrl, "="},
            {"Alt+Enter", SDL_SCANCODE_RETURN, kAlt, "\r"},
            {"Alt+Shift+M", SDL_SCANCODE_M, SDL_Keymod(kAlt | kShift), "M"},
        };
        for (const Row &r : chords) {
            DesktopInput in = input_in(m);
            run(in, key(r.sc, r.mod));
            const Out t = run(in, text(r.typed));
            check(none(t), format("A1 %s: the text after %s is suppressed", mode_tag(m), r.name), describe(t));
            // The suppression is one event long: the next press types normally.
            run(in, key(SDL_SCANCODE_K));
            const Out next = run(in, text("k"));
            check(is_char(next, 'k'), format("A2 %s: after %s the next key types again", mode_tag(m), r.name), describe(next));
        }
        {
            // Focus changes forget a pending suppression.
            DesktopInput in = input_in(m);
            run(in, key(SDL_SCANCODE_M, kAlt));
            const Out lost = run(in, plain(SDL_EVENT_WINDOW_FOCUS_LOST));
            const Out t = run(in, text("m"));
            check(none(lost) && is_char(t, 'm'), format("A3 %s: a focus change resets the text suppression", mode_tag(m)),
                  describe(lost) + " / " + describe(t));
        }
        // AltGr types characters: it is neither Alt nor Ctrl. SDL3 sends it as
        // Right Alt alone on a layout with an AltGr level; the older Ctrl+Right
        // Alt form is recognised on any layout.
        struct AltGr {
            const char *name;
            SDL_Scancode sc;
            const char *typed;
            char want; // 0 = nothing typed (non-ASCII)
        };
        const AltGr altgr[] = {
            {"AltGr+Q -> '@' (German)", SDL_SCANCODE_Q, "@", '@'},
            {"AltGr+7 -> '{' (German)", SDL_SCANCODE_7, "{", '{'},
            {"AltGr+M -> U+00B5 (German)", SDL_SCANCODE_M, "\xc2\xb5", 0},
            {"AltGr+S", SDL_SCANCODE_S, "\xc5\x9b", 0},
            {"AltGr+D", SDL_SCANCODE_D, "\xc4\x91", 0},
            {"AltGr+E -> euro", SDL_SCANCODE_E, "\xe2\x82\xac", 0},
            {"AltGr+=", SDL_SCANCODE_EQUALS, "~", '~'},
            {"AltGr+L", SDL_SCANCODE_L, "\xc5\x82", 0},
        };
        struct Form {
            const char *name;
            SDL_Keymod mod;
            bool layout_has_altgr;
        };
        const Form forms[] = {{"RAlt, AltGr layout", kAltGr, true}, {"Ctrl+RAlt, any layout", kAltGrLegacy, false}};
        for (const Form &f : forms) {
            for (const AltGr &a : altgr) {
                DesktopInput in = input_in(m);
                in.set_layout_has_altgr(f.layout_has_altgr);
                const Out down = run(in, key(a.sc, f.mod), ctx(UiMode::Dialogue, UiRequestId::None, false, false, true));
                const Out t = run(in, text(a.typed), ctx(UiMode::Dialogue, UiRequestId::None, false, false, true));
                const bool typed_ok = a.want ? is_char(t, a.want) : none(t);
                check(none(down) && typed_ok,
                      format("G1 %s (%s): %s is typing, not an Alt/Ctrl chord (no command on key-down; text %s)", mode_tag(m),
                             f.name, a.name, a.want ? "passes" : "non-ASCII, dropped"),
                      describe(down) + " / " + describe(t));
            }
            DesktopInput in = input_in(m);
            in.set_layout_has_altgr(f.layout_has_altgr);
            const Out enter = run(in, key(SDL_SCANCODE_RETURN, f.mod));
            check(is_game(enter, UiActionKind::Confirm),
                  format("G2 %s (%s): AltGr+Enter is Enter (Confirm), not Alt+Enter fullscreen", mode_tag(m), f.name),
                  describe(enter));
        }
        {
            // On an AltGr layout the LEFT Alt is still the T-Deck's Alt, and
            // AltGr+S / AltGr+L never quick-save or quick-load.
            DesktopInput in = input_in(m);
            in.set_layout_has_altgr(true);
            const Out lalt = run(in, key(SDL_SCANCODE_M, kAlt));
            const Out altgr_s = run(in, key(SDL_SCANCODE_S, kAltGr));
            const Out altgr_l = run(in, key(SDL_SCANCODE_L, kAltGr));
            check(is_game(lalt, UiActionKind::SystemMenu) && none(altgr_s) && none(altgr_l),
                  format("G3 %s: AltGr layout: Left Alt+M is the menu; AltGr+S / AltGr+L do nothing", mode_tag(m)),
                  describe(lalt) + " / " + describe(altgr_s) + " / " + describe(altgr_l));
        }
        {
            DesktopInput in = input_in(m);
            check(!in.layout_has_altgr(), format("G4 %s: a new DesktopInput assumes no AltGr level (Right Alt = Alt)", mode_tag(m)));
        }
    }
}

// SPP1 G: Ctrl-K / Ctrl-V are the DOS control codes 0x0B / 0x16 in both modes (the core
// prints the karma / "1.16"); Ctrl-B ("Buffer On/Off", a BIOS type-ahead switch) stays unmapped.
void dos_control_keys() {
    for (InterfaceMode m : kModes) {
        DesktopInput in = input_in(m);
        const Out k = run(in, key(SDL_SCANCODE_K, kCtrl));
        const Out k_text = run(in, text("k"));
        const Out v = run(in, key(SDL_SCANCODE_V, SDL_KMOD_RCTRL));
        const Out b = run(in, key(SDL_SCANCODE_B, kCtrl));
        check(is_char(k, 0x0b) && std::strcmp(k.c[0].source, "ctrl-k") == 0 && k.c[0].shortcut == DeviceShortcut::None,
              format("K1 %s: Ctrl+K -> Character 0x0B (source \"ctrl-k\")", mode_tag(m)), describe(k));
        check(none(k_text), format("K1 %s: the chord's own text event is suppressed", mode_tag(m)), describe(k_text));
        check(is_char(v, 0x16) && std::strcmp(v.c[0].source, "ctrl-v") == 0,
              format("K2 %s: Ctrl+V (either Ctrl) -> Character 0x16", mode_tag(m)), describe(v));
        check(none(b), format("K3 %s: Ctrl+B is unmapped", mode_tag(m)), describe(b));
    }
}

void misc() {
    DesktopInput in;
    check(in.mode() == InterfaceMode::Original, "X1 a new DesktopInput starts in Original mode");
    in.set_mode(InterfaceMode::Enhanced);
    check(in.mode() == InterfaceMode::Enhanced, "X2 set_mode(Enhanced) is reported by mode()");
    const SDL_EventType ignored[] = {SDL_EVENT_KEY_UP, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN,
                                     SDL_EVENT_TEXT_EDITING, SDL_EVENT_QUIT, SDL_EVENT_WINDOW_FOCUS_GAINED};
    bool all_none = true;
    for (SDL_EventType t : ignored) all_none = all_none && none(run(in, plain(t)));
    check(all_none, "X3 key-up, mouse, IME editing, quit and focus events produce no command");
    // A key-up of Enter must not confirm.
    SDL_Event up = key(SDL_SCANCODE_RETURN);
    up.type = SDL_EVENT_KEY_UP;
    up.key.type = SDL_EVENT_KEY_UP;
    up.key.down = false;
    check(none(run(in, up)), "X4 Enter's key-up is ignored (only key-down acts)");
    // Every Game command that is not a shortcut carries shortcut None, and the
    // default command source is "keyboard".
    const Out enter = run(in, key(SDL_SCANCODE_RETURN));
    check(enter.n == 1 && std::strcmp(enter.c[0].source, "keyboard") == 0 && enter.c[0].shortcut == DeviceShortcut::None,
          "X5 a plain game key reports source \"keyboard\" and no shortcut", describe(enter));
    // Unmapped function keys do nothing in either mode.
    for (InterfaceMode m : kModes) {
        DesktopInput d = input_in(m);
        const SDL_Scancode fk[] = {SDL_SCANCODE_F1, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F6};
        bool quiet = true;
        for (SDL_Scancode s : fk) quiet = quiet && none(run(d, key(s)));
        check(quiet, format("X6 %s: F1/F3/F4/F6 are unmapped", mode_tag(m)));
    }
}

} // namespace

int main() {
    std::printf("uv_desktop_input_test: uv::DesktopInput::translate over hand-built SDL_Events\n");
    directions_and_paging();
    confirm_cancel_back();
    repeat_policy();
    text_input();
    party_selection();
    system_mute_window();
    original_vs_enhanced();
    chords_and_altgr();
    dos_control_keys();
    misc();
    return uvtest::finish("uv_desktop_input_test");
}
