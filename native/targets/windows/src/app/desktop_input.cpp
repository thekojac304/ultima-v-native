#include "app/desktop_input.h"

#include <SDL3/SDL_keyboard.h>

namespace uv {
namespace {
using openu5::UiAction;
using openu5::UiActionKind;
using openu5::UiMode;
using tdeck::DeviceShortcut;

DesktopCommand game(UiAction action, const char *source = "keyboard") {
    DesktopCommand c;
    c.kind = DesktopCommand::Kind::Game;
    c.action = action;
    c.source = source;
    return c;
}

// A device shortcut carries no key. UiAction's default kind is Confirm, so a
// shortcut must not leave that default in place for a screen to read as
// Enter: a Character with no character is what no screen accepts.
DesktopCommand shortcut(DeviceShortcut s, const char *source) {
    DesktopCommand c;
    c.kind = DesktopCommand::Kind::Game;
    c.action.kind = UiActionKind::Character;
    c.action.character = 0;
    c.shortcut = s;
    c.source = source;
    return c;
}

DesktopCommand window(DesktopCommand::Kind kind) {
    DesktopCommand c;
    c.kind = kind;
    return c;
}

UiAction kind(UiActionKind k) {
    UiAction a{};
    a.kind = k;
    return a;
}

UiAction direction(openu5::Direction d) {
    UiAction a{};
    a.kind = UiActionKind::Direction;
    a.direction = d;
    return a;
}

UiAction character(char16_t c) {
    UiAction a{};
    a.kind = UiActionKind::Character;
    a.character = c;
    return a;
}

bool typing(UiMode mode) { return mode == UiMode::TextEntry || mode == UiMode::NumericEntry; }

// UiInputAdapter's movement contexts (ui_input_adapter.cpp is_movement_context).
bool movement_context(const InputContext &c) {
    return c.accepts_direction || c.mode == UiMode::Exploration || c.mode == UiMode::Dungeon ||
           c.mode == UiMode::Combat || c.mode == UiMode::TargetSelection;
}

size_t emit(DesktopCommand *out, size_t capacity, size_t n, const DesktopCommand &c) {
    if (n < capacity) out[n] = c;
    return n < capacity ? n + 1 : n;
}
} // namespace

size_t DesktopInput::translate(const SDL_Event &event, const InputContext &context, DesktopCommand *out,
                               size_t capacity) {
    if (!out || capacity == 0) return 0;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: return key_down(event.key, context, out, capacity);
    case SDL_EVENT_TEXT_INPUT: return text(event.text.text, context, out, capacity);
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_FOCUS_GAINED: reset(); return 0;
    default: return 0;
    }
}

size_t DesktopInput::key_down(const SDL_KeyboardEvent &key, const InputContext &context, DesktopCommand *out,
                              size_t capacity) {
    last_key_repeat_ = key.repeat;
    suppress_text_ = false;
    const SDL_Keymod mod = key.mod;
    const bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    // AltGr arrives as Ctrl+RAlt on European layouts and types characters: it
    // is neither the T-Deck's Alt nor a desktop Ctrl chord.
    const bool altgr = (mod & SDL_KMOD_RALT) != 0 && (mod & SDL_KMOD_CTRL) != 0;
    const bool alt = !altgr && (mod & SDL_KMOD_ALT) != 0;
    const bool ctrl = !altgr && (mod & SDL_KMOD_CTRL) != 0;
    const bool enhanced = mode_ == InterfaceMode::Enhanced;
    const SDL_Scancode sc = key.scancode;
    size_t n = 0;

    // --- Window management (never game keys) --------------------------------
    if ((alt && (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)) || sc == SDL_SCANCODE_F11) {
        suppress_text_ = true;
        return key.repeat ? 0 : emit(out, capacity, n, window(DesktopCommand::Kind::ToggleFullscreen));
    }
    if (ctrl && (sc == SDL_SCANCODE_EQUALS || sc == SDL_SCANCODE_KP_PLUS)) {
        suppress_text_ = true;
        return key.repeat ? 0 : emit(out, capacity, n, window(DesktopCommand::Kind::WindowScaleUp));
    }
    if (ctrl && (sc == SDL_SCANCODE_MINUS || sc == SDL_SCANCODE_KP_MINUS)) {
        suppress_text_ = true;
        return key.repeat ? 0 : emit(out, capacity, n, window(DesktopCommand::Kind::WindowScaleDown));
    }
    if (sc == SDL_SCANCODE_F12) {
        return key.repeat ? 0 : emit(out, capacity, n, window(DesktopCommand::Kind::Screenshot));
    }

    // --- Movement and paging (OS repeat allowed) ----------------------------
    openu5::Direction dir{};
    bool is_direction = true;
    switch (sc) {
    case SDL_SCANCODE_UP: dir = openu5::Direction::North; break;
    case SDL_SCANCODE_DOWN: dir = openu5::Direction::South; break;
    case SDL_SCANCODE_LEFT: dir = openu5::Direction::West; break;
    case SDL_SCANCODE_RIGHT: dir = openu5::Direction::East; break;
    case SDL_SCANCODE_KP_8: is_direction = (mod & SDL_KMOD_NUM) == 0; dir = openu5::Direction::North; break;
    case SDL_SCANCODE_KP_2: is_direction = (mod & SDL_KMOD_NUM) == 0; dir = openu5::Direction::South; break;
    case SDL_SCANCODE_KP_4: is_direction = (mod & SDL_KMOD_NUM) == 0; dir = openu5::Direction::West; break;
    case SDL_SCANCODE_KP_6: is_direction = (mod & SDL_KMOD_NUM) == 0; dir = openu5::Direction::East; break;
    default: is_direction = false; break;
    }
    if (is_direction && !alt && !ctrl) {
        suppress_text_ = true; // numpad digits with NumLock off type nothing
        // The T-Deck's Shift + trackball up/down pages the console.
        if (shift && (dir == openu5::Direction::North || dir == openu5::Direction::South))
            return emit(out, capacity, n,
                        game(kind(dir == openu5::Direction::North ? UiActionKind::PageUp : UiActionKind::PageDown)));
        return emit(out, capacity, n, game(direction(dir)));
    }
    if (sc == SDL_SCANCODE_PAGEUP && !alt && !ctrl) return emit(out, capacity, n, game(kind(UiActionKind::PageUp)));
    if (sc == SDL_SCANCODE_PAGEDOWN && !alt && !ctrl) return emit(out, capacity, n, game(kind(UiActionKind::PageDown)));
    if (sc == SDL_SCANCODE_BACKSPACE && !alt && !ctrl) {
        suppress_text_ = true;
        return emit(out, capacity, n, game(kind(typing(context.mode) ? UiActionKind::DeleteCharacter : UiActionKind::Back)));
    }

    if (key.repeat) {
        // Every other key acts once per press, as on the T-Deck.
        suppress_text_ = true;
        return 0;
    }

    // --- Confirm / cancel -----------------------------------------------------
    if ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && !alt && !ctrl) {
        suppress_text_ = true;
        return emit(out, capacity, n, game(kind(UiActionKind::Confirm)));
    }
    if (sc == SDL_SCANCODE_ESCAPE && !alt && !ctrl) {
        suppress_text_ = true;
        return emit(out, capacity, n, game(kind(UiActionKind::Cancel)));
    }

    // --- System keys ----------------------------------------------------------
    if (sc == SDL_SCANCODE_F10 && !alt && !ctrl) return emit(out, capacity, n, game(kind(UiActionKind::SystemMenu), "f10"));
    if (sc == SDL_SCANCODE_F7) return emit(out, capacity, n, shortcut(DeviceShortcut::MusicMute, "f7"));
    if (sc == SDL_SCANCODE_F8) return emit(out, capacity, n, shortcut(DeviceShortcut::SfxMute, "f8"));
    if (ctrl && !alt) {
        suppress_text_ = true;
        // The DOS game's own Ctrl-S toggles the sound; Ctrl-M the same for music.
        if (sc == SDL_SCANCODE_S) return emit(out, capacity, n, shortcut(DeviceShortcut::SfxMute, "ctrl-s"));
        if (sc == SDL_SCANCODE_M) return emit(out, capacity, n, shortcut(DeviceShortcut::MusicMute, "ctrl-m"));
        return 0;
    }
    if (enhanced) {
        if (sc == SDL_SCANCODE_F5 && !alt) return emit(out, capacity, n, shortcut(DeviceShortcut::Save, "f5"));
        if (sc == SDL_SCANCODE_F9 && !alt) return emit(out, capacity, n, shortcut(DeviceShortcut::Load, "f9"));
        if (sc == SDL_SCANCODE_F2 && !alt)
            return emit(out, capacity, n, shortcut(DeviceShortcut::MovementModeToggled, "f2"));
    }
    if (alt) {
        suppress_text_ = true;
        // The T-Deck's Alt chords (ui_input_adapter.cpp), kept for players of
        // both versions: Alt+M menu and Alt+Shift+M/S mutes in every mode.
        if (sc == SDL_SCANCODE_M && shift) return emit(out, capacity, n, shortcut(DeviceShortcut::MusicMute, "alt-shift-m"));
        if (sc == SDL_SCANCODE_S && shift) return emit(out, capacity, n, shortcut(DeviceShortcut::SfxMute, "alt-shift-s"));
        if (sc == SDL_SCANCODE_M) return emit(out, capacity, n, game(kind(UiActionKind::SystemMenu), "alt-m"));
        if (enhanced && sc == SDL_SCANCODE_S) return emit(out, capacity, n, shortcut(DeviceShortcut::Save, "alt-s"));
        if (enhanced && sc == SDL_SCANCODE_L) return emit(out, capacity, n, shortcut(DeviceShortcut::Load, "alt-l"));
        if (sc == SDL_SCANCODE_D && (enhanced || context.developer_tools))
            return emit(out, capacity, n, shortcut(DeviceShortcut::DeveloperMenu, "alt-d"));
        return 0;
    }
    // Everything else arrives as SDL_EVENT_TEXT_INPUT.
    return 0;
}

size_t DesktopInput::text(const char *utf8, const InputContext &context, DesktopCommand *out, size_t capacity) {
    const bool suppressed = suppress_text_ || last_key_repeat_;
    suppress_text_ = false;
    last_key_repeat_ = false;
    if (suppressed || !utf8) return 0;
    size_t n = 0;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(utf8); *p; ++p) {
        const unsigned char c = *p;
        // The game reads 7-bit ASCII (RawInputEvent codes); anything else --
        // a multi-byte UTF-8 sequence from a non-English layout or an IME --
        // has no Ultima V key and is dropped, byte by byte.
        if (c < 0x20 || c > 0x7e) continue;
        // DOS "Player:" prompts take the member's number. The Camp watch and
        // the Mix reagent picker read their digits as characters themselves.
        if (context.mode == UiMode::PartySelection && c >= '1' && c <= '9' &&
            context.request != openu5::UiRequestId::CampGuard && context.request != openu5::UiRequestId::MixReagents) {
            UiAction a{};
            a.kind = UiActionKind::SelectIndex;
            a.index = int32_t(c - '1');
            n = emit(out, capacity, n, game(a));
            continue;
        }
        // Enhanced WASD Movement Mode, exactly UiInputAdapter's rule.
        if (mode_ == InterfaceMode::Enhanced && context.movement_mode && movement_context(context)) {
            const char lower = char(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            bool moved = true;
            openu5::Direction d{};
            if (lower == 'w') d = openu5::Direction::North;
            else if (lower == 'a') d = openu5::Direction::West;
            else if (lower == 's') d = openu5::Direction::South;
            else if (lower == 'd') d = openu5::Direction::East;
            else moved = false;
            if (moved) {
                n = emit(out, capacity, n, game(direction(d), "wasd"));
                continue;
            }
        }
        n = emit(out, capacity, n, game(character(char16_t(c))));
    }
    return n;
}

} // namespace uv
