#pragma once
// Ultima V Native for Windows: the desktop keyboard front end.
//
// The T-Deck turns its keyboard matrix and trackball into openu5::UiAction /
// tdeck::DeviceShortcut values (native/targets/tdeck/main/ui_input_adapter.cpp)
// and AlphaRuntime routes them. A desktop keyboard has keys the T-Deck lacks
// (arrows, Escape, PgUp/PgDn, function keys) and lacks the T-Deck's (trackball,
// Mic, Sym), so it gets its own translation -- to the SAME values, handed to
// AlphaRuntime::handle_action(), which routes them exactly like a T-Deck key.
// No gameplay rule lives here: only which key means which existing action.
//
// The translation is mode-aware (DesktopSettings::interface_mode):
//   both modes  arrows = directions (Shift+Up/Down page the console), Enter =
//               Confirm, Esc = Cancel, Backspace = Back (DeleteCharacter while
//               typing), PgUp/PgDn = page, letters/digits/Space/punctuation =
//               the original command keys, digits 1-6 pick the member in a
//               "Player:" prompt, F10 / Alt+M = System Menu, F7 / F8 and
//               Ctrl+M / Ctrl+S = music / sound off and on (the DOS game's
//               Ctrl-S sound toggle), Alt+Enter / F11 = fullscreen.
//   Enhanced    adds F5 / Alt+S quick save, F9 / Alt+L quick load, F2 WASD
//               Movement Mode (W/A/S/D move while it is on), Alt+D Developer.
//   Original    none of the Enhanced keys: W/A/S/D are always their letters.
#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_events.h>

#include "app/desktop_settings.h"
#include "openu5/ui_session.h"
#include "ui_input_adapter.h"

namespace uv {

/** What the runtime looks like to the keyboard right now (read before each event). */
struct InputContext {
    openu5::UiMode mode = openu5::UiMode::Exploration;   // AlphaRuntime::input_mode()
    openu5::UiRequestId request = openu5::UiRequestId::None;
    bool accepts_direction = false;                      // AlphaRuntime::accepts_direction_input()
    bool movement_mode = false;                          // AlphaRuntime::movement_mode()
    bool developer_tools = false;                        // Alt+D allowed (Enhanced, or --developer)
};

struct DesktopCommand {
    enum class Kind : uint8_t {
        None,
        Game,             // route `action` / `shortcut` through AlphaRuntime::handle_action
        ToggleFullscreen,
        WindowScaleUp,
        WindowScaleDown,
        Screenshot,
    };
    Kind kind = Kind::None;
    openu5::UiAction action{};
    tdeck::DeviceShortcut shortcut = tdeck::DeviceShortcut::None;
    const char *source = "keyboard";
};

class DesktopInput {
  public:
    static constexpr size_t kMaxCommands = 4;

    void set_mode(InterfaceMode mode) { mode_ = mode; }
    InterfaceMode mode() const { return mode_; }

    /**
     * Translates one SDL event (key down or text input; everything else is
     * ignored) into at most kMaxCommands commands; returns how many.
     */
    size_t translate(const SDL_Event &event, const InputContext &context, DesktopCommand *out, size_t capacity);

    /** Forget a pending text suppression (focus changes). */
    void reset() { suppress_text_ = false; last_key_repeat_ = false; }

  private:
    size_t key_down(const SDL_KeyboardEvent &key, const InputContext &context, DesktopCommand *out, size_t capacity);
    size_t text(const char *utf8, const InputContext &context, DesktopCommand *out, size_t capacity);

    InterfaceMode mode_ = InterfaceMode::Original;
    // A key handled on key-down (an Alt or Ctrl chord) must not also arrive as text.
    bool suppress_text_ = false;
    // OS auto-repeat: allowed for movement, paging and Backspace only (the
    // T-Deck has none; a held letter must not repeat a command).
    bool last_key_repeat_ = false;
};

} // namespace uv
