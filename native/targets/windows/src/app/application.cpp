#include "app/application.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <windows.h>
#include <shellapi.h>

#include <SDL3/SDL.h>

#include "alpha_audio.h"
#include "alpha_resources.h"
#include "alpha_runtime.h"
#include "app/autotest.h"
#include "app/data_paths.h"
#include "app/desktop_input.h"
#include "app/desktop_profile.h"
#include "app/presenter.h"
#include "app/sdl_audio_backend.h"
#include "asset_pack.h"
#include "build_info.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "openu5/inventory.h"
#include "openu5/outdoor.h"
#include "openu5/world.h"
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
#include "openu5/debug_map_picker.h"
#endif
#include "platform/log.h"
#include "platform/storage.h"
#include "platform/virtual_panel.h"
#include "tdeck_board.h"

namespace uv {
namespace {
constexpr char kTag[] = "UltimaV-Windows";

std::string narrow(const std::wstring &w) { return storage::wide_to_utf8(w); }

// ---------------------------------------------------------------------------
// Messages the player sees when the game cannot start
// ---------------------------------------------------------------------------
void show_error(SDL_Window *window, const std::string &title, const std::string &text) {
    ESP_LOGE(kTag, "%s: %s", title.c_str(), text.c_str());
    log::flush();
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), window);
}

/** A missing or mismatched pack: explain, and offer to open the folder the packs go in. */
void show_asset_problem(SDL_Window *window, const DataPaths &paths, const std::string &detail) {
    const std::string folder = narrow(paths.ultima5_dir());
    const std::string text =
        detail +
        "\n\nUltima V Native plays from resource packs made from YOUR OWN copy of Ultima V; no game data "
        "is included with it. The packs go in:\n\n  " +
        folder +
        "\\openu5-assets.bin\n  " + folder +
        "\\openu5-alpha1-resources.bin\n  " + folder +
        "\\openu5-audio.bin   (optional: music, from the music-patched game)\n\n"
        "README-Windows.txt, section \"Preparing the game data\", explains how to make them -- or copy the "
        "ultima5 folder of a T-Deck SD card prepared for this version.";
    ESP_LOGE(kTag, "ASSETS %s", detail.c_str());
    log::flush();
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Open the packs folder"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
    };
    const SDL_MessageBoxData box{SDL_MESSAGEBOX_ERROR, window, "Ultima V Native: game data needed", text.c_str(),
                                 SDL_arraysize(buttons), buttons, nullptr};
    int chosen = 0;
    if (SDL_ShowMessageBox(&box, &chosen) && chosen == 1) {
        storage::ensure_directory(paths.ultima5_dir());
        ShellExecuteW(nullptr, L"open", paths.ultima5_dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

std::string pack_identity(const char *what, uint16_t major, uint16_t minor, uint32_t size, uint32_t crc) {
    char line[160];
    std::snprintf(line, sizeof(line), "%s v%u.%u, %lu bytes, CRC %08lx", what, unsigned(major), unsigned(minor),
                  (unsigned long)size, (unsigned long)crc);
    return line;
}

// ---------------------------------------------------------------------------
// Single instance per data directory: two windows on one "card" would
// interleave save generations. The lock is a file inside the folder, opened
// with no sharing and deleted when its handle closes (also on a crash), so
// every spelling of the folder -- case, 8.3 names, subst or mapped drives,
// another logon session, a network share -- meets the same lock.
// ---------------------------------------------------------------------------
class InstanceLock {
  public:
    explicit InstanceLock(const std::wstring &data_dir) {
        const std::wstring path = data_dir + L"\\instance.lock";
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            handle_ = nullptr;
            const DWORD error = GetLastError();
            // Only "someone holds it" refuses the start; a folder that cannot
            // take the file at all has already failed the writability check.
            owned_ = error != ERROR_SHARING_VIOLATION;
            if (owned_) ESP_LOGW(kTag, "instance lock unavailable (error %lu); continuing", (unsigned long)error);
        } else {
            owned_ = true;
        }
    }
    ~InstanceLock() {
        if (handle_) CloseHandle(handle_);
    }
    bool owned() const { return owned_; }

  private:
    HANDLE handle_ = nullptr;
    bool owned_ = false;
};

const char *ui_mode_label(openu5::UiMode mode) {
    switch (mode) {
    case openu5::UiMode::Exploration: return "exploration";
    case openu5::UiMode::Dungeon: return "dungeon";
    case openu5::UiMode::Combat: return "combat";
    case openu5::UiMode::Dialogue: return "dialogue";
    case openu5::UiMode::Shop: return "shop";
    default: return "other";
    }
}

// ---------------------------------------------------------------------------
// The running game
// ---------------------------------------------------------------------------
class Game {
  public:
    explicit Game(const AppOptions &options) : options_(options) {}
    int run();

  private:
    bool boot();
    void loop();
    void shutdown();
    /** One pass: events, render, present. False = quit requested. */
    bool pass(bool may_wait);
    void handle_event(const SDL_Event &event);
    /** Closing the window mid-journey: ask first (unsaved progress). True = quit. */
    bool confirm_quit();
    void apply_command(const DesktopCommand &command);
    InputContext input_context() const;
    void sync_mode();
    void apply_game_screen();
    static void on_profile_change(void *context, DesktopProfile::Change what);
    static uint8_t largest_window_scale(void *context);
    void install_profile();
    void follow_display_scale();
    void open_data_folder();
    void update_title();
    void save_settings();
    void take_screenshot();
    bool run_autotest();
    bool autotest_expect(const AutotestStep &step);
    void autotest_dump(int line);
    void autotest_key(const char *name, std::vector<std::string> mods);
    void autotest_type(const std::string &text);
    bool autotest_teleport(const AutotestStep &step);
    bool autotest_fight(const AutotestStep &step);
    bool autotest_shop(const AutotestStep &step);
    bool autotest_give(const AutotestStep &step);
    std::string right_column_signature() const;
    std::string dos_screen_check(const std::string &value) const;
    bool autotest_open_grass(int &gx, int &gy);
    void push_key(const std::string &name, const std::vector<std::string> &mods);
    void push_text(const std::string &text);

    AppOptions options_;
    DataPaths paths_{};
    DesktopSettings settings_{};
    // windows.json as the player left it. --mode / --fullscreen / --windowed
    // apply to this run only: until the player changes that setting in the
    // game, save_settings() writes the stored value back, not the override.
    DesktopSettings stored_settings_{};
    bool mode_overridden_ = false;
    bool fullscreen_overridden_ = false;
    // settings_.window_scale is the scale the window HAS (what the Display
    // row shows; it follows a drag or a snap). windows.json gets the scale
    // the player last chose (Display row, Ctrl+= / Ctrl+-) as the window got
    // it; until the player picks one in this run (kAutoWindowScale here) it
    // keeps the stored value -- automatic, or a choice made on a larger
    // display -- so a start on a smaller display never shrinks the saved
    // choice, and a drag is never saved.
    uint8_t chosen_window_scale_ = kAutoWindowScale;
    std::unique_ptr<InstanceLock> lock_;
    Presenter presenter_;
    DesktopInput input_;
    DesktopProfile profile_;
    DosTextTables dos_text_; // Original mode's DOS list names, from the pack's data.json
    std::unique_ptr<tdeck::Board> board_;
    std::unique_ptr<tdeck::AlphaRuntime> runtime_;
    std::unique_ptr<SdlAudioBackend> audio_;
    tdeck::RetainedAudioPayload retained_audio_{};
    bool sdl_ready_ = false;
    bool ready_ = false;
    bool quit_ = false;
    int exit_code_ = 0;
    int64_t heartbeat_us_ = 0;
    InterfaceMode applied_mode_ = InterfaceMode::Original;
    uint64_t expect_generation_ = 0;
    int autotest_failures_ = 0;
    // Pushed text events point at their characters until they are polled.
    std::deque<std::string> text_buffers_;
    std::string remembered_position_;
    uint32_t remembered_combat_actions_ = 0;
    int64_t remembered_turns_ = 0;
};

int Game::run() {
    if (boot()) {
        if (!options_.autotest_script.empty()) {
            const bool ok = run_autotest();
            exit_code_ = ok ? 0 : std::max(1, autotest_failures_);
        } else {
            loop();
        }
    }
    shutdown();
    return exit_code_;
}

bool Game::boot() {
    // --- Data directory and log ------------------------------------------------
    paths_ = resolve_data_paths(options_.data_dir);
    storage::set_root(paths_.data_dir);
    const bool dirs_ok = storage::ensure_directory(paths_.ultima5_dir());
    log::set_console(options_.console);
    if (options_.log_level >= 0) log::set_level(options_.log_level);
    if (dirs_ok) log::open_file(paths_.logs_dir() + L"\\windows.log");
    ESP_LOGI(kTag, "========================================");
    ESP_LOGI(kTag, "%s for Windows %s (git %s, %s, built %s)", build::kProductName, build::kVersion,
             build::kGitCommit, build::kCompiler, build::kBuildDate);
    ESP_LOGI(kTag, "data directory: %s (%s)", narrow(paths_.data_dir).c_str(), data_dir_source_name(paths_.source));

    // --- SDL ---------------------------------------------------------------------
    SDL_SetAppMetadata(build::kProductName, build::kVersion, "org.ultimavnative.windows");
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        show_error(nullptr, "Ultima V Native", std::string("SDL could not start: ") + SDL_GetError());
        exit_code_ = 2;
        return false;
    }
    sdl_ready_ = true;
    ESP_LOGI(kTag, "SDL %d.%d.%d video=%s", SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
             SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()),
             SDL_GetCurrentVideoDriver());

    if (!dirs_ok) {
        show_error(nullptr, "Ultima V Native",
                   "The data folder cannot be created or written:\n\n  " + narrow(paths_.ultima5_dir()) +
                       "\n\nStart with --data-dir <folder> to use another location.");
        exit_code_ = 3;
        return false;
    }
    lock_ = std::make_unique<InstanceLock>(paths_.data_dir);
    if (!lock_->owned()) {
        show_error(nullptr, "Ultima V Native",
                   "Ultima V Native is already running with this data folder:\n\n  " + narrow(paths_.data_dir));
        exit_code_ = 4;
        return false;
    }

    // --- Desktop settings ----------------------------------------------------------
    if (!load_desktop_settings(paths_.ultima5_dir() + L"\\windows.json", settings_))
        ESP_LOGW(kTag, "windows.json is malformed; the desktop defaults are used");
    stored_settings_ = settings_;
    mode_overridden_ = options_.interface_override;
    fullscreen_overridden_ = options_.force_fullscreen || options_.force_windowed;
    if (options_.interface_override) settings_.interface_mode = options_.interface_mode;
    if (options_.screen_override) settings_.original_screen = options_.original_screen;
    if (options_.force_fullscreen) settings_.fullscreen = true;
    if (options_.force_windowed) settings_.fullscreen = false;
    applied_mode_ = settings_.interface_mode;
    input_.set_mode(settings_.interface_mode);
    install_profile();

    // --- Window --------------------------------------------------------------------
    if (!presenter_.create(build::kProductName, settings_)) {
        show_error(nullptr, "Ultima V Native", "The game window could not be created.\n\n" + presenter_.error());
        exit_code_ = 5;
        return false;
    }
    settings_.window_scale = presenter_.window_scale();
    update_title();
    input_.set_layout_has_altgr(DesktopInput::detect_layout_altgr());
    ESP_LOGI(kTag, "keyboard layout has AltGr: %s", input_.layout_has_altgr() ? "yes" : "no");

    // --- The shared presentation on the virtual panel ---------------------------------
    panel::reset(0x0000);
    board_ = std::make_unique<tdeck::Board>();
    if (board_->initialize_display() != ESP_OK) {
        show_error(presenter_.window(), "Ultima V Native", "The display could not be initialised (see the log).");
        exit_code_ = 6;
        return false;
    }
    apply_game_screen();

    // --- Resource packs, with the T-Deck's identity gate (main.cpp) ---------------------
    openu5::AssetPackReader tile_pack;
    openu5::AssetPackReport tile_report{};
    tdeck::AlphaResourcePack alpha_pack;
    tdeck::AlphaResourceReport alpha_report{};
    const esp_err_t tiles = tile_pack.open(openu5::kAssetPackPath, tile_report);
    const esp_err_t alpha = alpha_pack.open(tdeck::kAlphaResourcePath, alpha_report);
    ESP_LOGI(kTag, "IDENTITY asset=%s alpha=%s", esp_err_to_name(tiles), esp_err_to_name(alpha));
    if (tiles != ESP_OK || alpha != ESP_OK) {
        std::string detail = "The game's resource packs are missing or unreadable.";
        if (tiles != ESP_OK && alpha == ESP_OK) detail = "openu5-assets.bin is missing or unreadable.";
        if (alpha != ESP_OK && tiles == ESP_OK) detail = "openu5-alpha1-resources.bin is missing or unreadable.";
        show_asset_problem(presenter_.window(), paths_, detail);
        exit_code_ = 10;
        return false;
    }
    if (!tile_report.firmware_match || !alpha_report.firmware_match) {
        std::string detail = "The resource packs were made by a different version of the pack builder than this "
                             "build of the game expects, so they cannot be used.\n\n";
        detail += "Found:    " + pack_identity("assets", tile_report.version_major, tile_report.version_minor,
                                               tile_report.file_size, tile_report.payload_crc32) + "\n";
        detail += "Expected: " + pack_identity("assets", tile_report.version_major, tile_report.version_minor,
                                               openu5::kExpectedAssetPackSize, openu5::kExpectedAssetPackCrc32) + "\n";
        detail += "Found:    " + pack_identity("resources", alpha_report.version_major, alpha_report.version_minor,
                                               alpha_report.file_size, alpha_report.payload_crc32) + "\n";
        detail += "Expected: " + pack_identity("resources", alpha_report.version_major, alpha_report.version_minor,
                                               tdeck::kExpectedAlphaResourceSize, tdeck::kExpectedAlphaResourceCrc32);
        show_asset_problem(presenter_.window(), paths_, detail);
        exit_code_ = 11;
        return false;
    }

    // Original mode's list panels print the DOS game's own names: the player's
    // DATA.OVL tables, as the pack's data.json carries them (app/dos_text.h).
    if (!dos_text_.load(alpha_pack)) ESP_LOGW(kTag, "DOS_TEXT data.json tables unavailable; lists keep the device names");
    else ESP_LOGI(kTag, "DOS_TEXT tables loaded");
    runtime_ = std::make_unique<tdeck::AlphaRuntime>();
    const esp_err_t initialized = runtime_->initialize(alpha_pack, alpha_report, tile_pack, tile_report);
    alpha_pack.close();
    tile_pack.close();
    if (initialized != ESP_OK) {
        show_error(presenter_.window(), "Ultima V Native",
                   std::string("The game could not start: ") + esp_err_to_name(initialized) + " (see the log in " +
                       narrow(paths_.logs_dir()) + ").");
        exit_code_ = 12;
        return false;
    }

    // --- Audio (optional, never part of the identity gate) ----------------------------
    const openu5::AudioPackInfo audio_pack = tdeck::load_audio_pack_info(tdeck::kAudioPackPath, &retained_audio_);
    ESP_LOGI(kTag, "AUDIO_PACK state=%s size=%lu capability=%s songs=%u",
             openu5::audio_pack_state_name(audio_pack.state), (unsigned long)audio_pack.file_size,
             audio_pack.state == openu5::AudioPackState::Valid
                 ? openu5::music_capability_name(audio_pack.record.capability)
                 : "none",
             unsigned(audio_pack.song_entries));
    if (options_.no_audio) {
        runtime_->configure_audio(audio_pack, nullptr);
        ESP_LOGI(kTag, "audio disabled (--no-audio)");
    } else {
        audio_ = std::make_unique<SdlAudioBackend>();
        audio_->set_music_library(retained_audio_.bytes ? &retained_audio_.payload : nullptr);
        if (!audio_->start()) ESP_LOGW(kTag, "no audio output: %s", audio_->error().c_str());
        runtime_->configure_audio(audio_pack, audio_.get());
        runtime_->attach_audio_perf(audio_.get());
    }
    // The T-Deck's keyboard backlight row: there is none on a desktop.
    runtime_->attach_keyboard_light({nullptr, nullptr, false});

    // --- First frame -------------------------------------------------------------------
    runtime_->render(*board_, true);
    presenter_.set_picture_height(board_->picture_height());
    presenter_.show();
    presenter_.present(true);
    SDL_StartTextInput(presenter_.window());
    heartbeat_us_ = esp_timer_get_time() + 5000000;
    ready_ = true;
    ESP_LOGI(kTag, "ready: interface=%s screen=%s window=%ux (%s) scaling=%s fullscreen=%d",
             interface_mode_name(settings_.interface_mode), board_->dos_layout() ? "DOS 320x200" : "handheld 320x240",
             unsigned(settings_.window_scale),
             stored_settings_.window_scale == kAutoWindowScale ? "automatic" : "saved", scaling_name(settings_.scaling),
             settings_.fullscreen);
    log::flush();
    return true;
}

InputContext Game::input_context() const {
    InputContext c;
    c.mode = runtime_->input_mode();
    c.request = runtime_->ui() ? runtime_->ui()->request() : openu5::UiRequestId::None;
    c.accepts_direction = runtime_->accepts_direction_input();
    c.movement_mode = runtime_->movement_mode();
    c.developer_tools = options_.developer;
    return c;
}

void Game::sync_mode() {
    if (settings_.interface_mode == applied_mode_) return;
    applied_mode_ = settings_.interface_mode;
    mode_overridden_ = false; // the player chose it: it is saved
    input_.set_mode(applied_mode_);
    install_profile();
    apply_game_screen();
    update_title();
    save_settings();
    ESP_LOGI(kTag, "INTERFACE_MODE %s", interface_mode_name(applied_mode_));
}

// Original's game screen is the 1988 game's 320x200 one (Board::set_dos_layout);
// the window is shaped for it -- "Window N x" is 320N x 200N there -- and for
// the handheld's 320x240 in Enhanced (or with "originalScreen": "Handheld").
void Game::apply_game_screen() {
    const bool dos = openu5::platform_dos_screen();
    if (board_) board_->set_dos_layout(dos);
    presenter_.set_window_shape(window_picture_height(settings_.interface_mode, settings_.original_screen));
    settings_.window_scale = presenter_.window_scale();
    ESP_LOGI(kTag, "GAME_SCREEN %s", dos ? "DOS 320x200" : "handheld 320x240");
}

void Game::install_profile() {
    DesktopProfile::Hooks hooks;
    hooks.settings = &settings_;
    hooks.context = this;
    hooks.changed = &Game::on_profile_change;
    hooks.largest_window_scale = &Game::largest_window_scale;
    hooks.dos_text = &dos_text_;
    profile_.install(hooks);
}

uint8_t Game::largest_window_scale(void *context) { return static_cast<Game *>(context)->presenter_.largest_window_scale(); }

// A desktop Settings row (Interface, Display, Scaling) changed settings_.
void Game::on_profile_change(void *context, DesktopProfile::Change what) {
    auto *self = static_cast<Game *>(context);
    switch (what) {
    case DesktopProfile::Change::Interface: self->sync_mode(); return; // persists too
    case DesktopProfile::Change::Display:
        self->fullscreen_overridden_ = false;
        self->presenter_.set_fullscreen(self->settings_.fullscreen);
        if (!self->settings_.fullscreen)
            self->chosen_window_scale_ = self->settings_.window_scale =
                self->presenter_.set_window_scale(self->settings_.window_scale);
        break;
    case DesktopProfile::Change::Scaling:
        self->presenter_.set_scaling(self->settings_.scaling);
        self->settings_.window_scale = self->presenter_.window_scale();
        break;
    }
    self->save_settings();
}

// The window's display scale changed (it was dragged to another monitor, or
// Windows' scale setting changed). A scale the player never chose follows
// it, as the first run's default would; a chosen one stays (Windows keeps
// the window's size in pixels). Fullscreen and maximised windows are left alone.
void Game::follow_display_scale() {
    if (chosen_window_scale_ != kAutoWindowScale || stored_settings_.window_scale != kAutoWindowScale) return;
    if (presenter_.fullscreen() || presenter_.maximized()) return;
    const uint8_t want = presenter_.default_window_scale();
    if (want == settings_.window_scale) return;
    settings_.window_scale = presenter_.set_window_scale(want);
    ESP_LOGI(kTag, "WINDOW_SCALE follows the display scale: %ux", unsigned(settings_.window_scale));
}

// The data folder itself: the game's files are in its ultima5 subfolder (the
// T-Deck card's layout), F12's pictures in its screenshots subfolder.
void Game::open_data_folder() {
    storage::ensure_directory(paths_.ultima5_dir());
    ShellExecuteW(nullptr, L"open", paths_.data_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    ESP_LOGI(kTag, "opened the data folder %s", narrow(paths_.data_dir).c_str());
}

void Game::update_title() {
    if (!presenter_.window()) return;
    const std::string title =
        std::string(build::kProductName) + " - " + interface_mode_name(settings_.interface_mode) + " mode";
    SDL_SetWindowTitle(presenter_.window(), title.c_str());
}

void Game::save_settings() {
    DesktopSettings out = settings_;
    if (mode_overridden_) out.interface_mode = stored_settings_.interface_mode;
    if (fullscreen_overridden_) out.fullscreen = stored_settings_.fullscreen;
    // Only windows.json chooses the Original screen (--original-screen is this run's).
    out.original_screen = stored_settings_.original_screen;
    out.window_scale = chosen_window_scale_ != kAutoWindowScale ? chosen_window_scale_ : stored_settings_.window_scale;
    if (save_desktop_settings(paths_.ultima5_dir() + L"\\windows.json", out)) stored_settings_ = out;
    else ESP_LOGW(kTag, "windows.json could not be written");
}

void Game::take_screenshot() {
    const std::wstring dir = paths_.data_dir + L"\\screenshots";
    storage::ensure_directory(dir);
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf(name, 64, L"\\ultima5-%04u%02u%02u-%02u%02u%02u.bmp", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    const std::wstring path = dir + name;
    const bool ok = presenter_.save_screenshot(path);
    ESP_LOGI(kTag, "SCREENSHOT %s %s", ok ? "saved" : "FAILED", narrow(path).c_str());
}

void Game::apply_command(const DesktopCommand &command) {
    switch (command.kind) {
    case DesktopCommand::Kind::Game: {
        const int64_t now = esp_timer_get_time();
        runtime_->handle_action(command.action, command.shortcut, now, command.source);
        break;
    }
    case DesktopCommand::Kind::ToggleFullscreen:
        fullscreen_overridden_ = false;
        settings_.fullscreen = !presenter_.fullscreen();
        presenter_.set_fullscreen(settings_.fullscreen);
        save_settings();
        break;
    case DesktopCommand::Kind::WindowScaleUp:
    case DesktopCommand::Kind::WindowScaleDown: {
        // 1x .. the largest that fits, as the Display row steps.
        const int delta = command.kind == DesktopCommand::Kind::WindowScaleUp ? 1 : -1;
        const int wanted = std::clamp(int(settings_.window_scale) + delta, int(kMinWindowScale),
                                      int(presenter_.largest_window_scale()));
        if (settings_.fullscreen) {
            fullscreen_overridden_ = false;
            settings_.fullscreen = false;
            presenter_.set_fullscreen(false);
        }
        chosen_window_scale_ = settings_.window_scale = presenter_.set_window_scale(uint8_t(wanted));
        save_settings();
        break;
    }
    case DesktopCommand::Kind::Screenshot: take_screenshot(); break;
    case DesktopCommand::Kind::OpenDataFolder: open_data_folder(); break;
    case DesktopCommand::Kind::None: break;
    }
}

bool Game::confirm_quit() {
    // On the title screens (and in scripted runs) there is nothing to lose.
    if (!options_.autotest_script.empty() || runtime_->frontend_open()) return true;
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Keep playing"},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Quit"},
    };
    const SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING, presenter_.window(), "Quit Ultima V Native?",
                                 "Progress since your last save will be lost.\n\n"
                                 "To save first: F10 -> Save Game (or Q in Original mode, F5 in Enhanced mode).",
                                 SDL_arraysize(buttons), buttons, nullptr};
    int chosen = 1;
    if (!SDL_ShowMessageBox(&box, &chosen)) return true;
    return chosen == 1;
}

void Game::handle_event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        if (confirm_quit()) quit_ = true;
        else presenter_.invalidate();
        return;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESTORED:
        // A drag, a snap, a restore: the Display row shows the scale the
        // picture has now (not saved: save_settings), and a scaling chosen
        // while the window was maximised gives it its shape.
        presenter_.window_changed();
        settings_.window_scale = presenter_.window_scale();
        return;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_RENDER_TARGETS_RESET:
    case SDL_EVENT_RENDER_DEVICE_RESET: presenter_.invalidate(); return;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        presenter_.invalidate();
        follow_display_scale();
        return;
    default: break;
    }
    DesktopCommand commands[DesktopInput::kMaxCommands];
    const size_t n = input_.translate(event, input_context(), commands, DesktopInput::kMaxCommands);
    for (size_t i = 0; i < n; ++i) apply_command(commands[i]);
}

bool Game::pass(bool may_wait) {
    const int64_t t0 = esp_timer_get_time();
    SDL_Event event;
    int drained = 0;
    bool input = false;
    while (drained < 64 && SDL_PollEvent(&event)) {
        ++drained;
        input = true;
        handle_event(event);
        if (quit_) return false;
    }
    sync_mode();
    if (input) runtime_->render(*board_);
    runtime_->render(*board_);
    presenter_.set_picture_height(board_->picture_height());
    presenter_.present();
    const int64_t now = esp_timer_get_time();
    if (now >= heartbeat_us_) {
        runtime_->log_metrics("heartbeat");
        log::flush();
        heartbeat_us_ = now + 5000000;
    }
    runtime_->note_loop_pass(uint32_t(esp_timer_get_time() - t0));
    if (!may_wait) return true;
    // The device's own pacing: block for its 10 ms tick while nothing is
    // paced against "now"; otherwise come straight back (a 1 ms sleep, not
    // the device's reschedule-spin, so a paced scene does not take a core).
    const uint32_t ticks = runtime_->loop_wait_ticks();
    const int64_t w0 = esp_timer_get_time();
    if (ticks) {
        const bool woke = SDL_WaitEventTimeout(nullptr, int(ticks * portTICK_PERIOD_MS));
        runtime_->note_loop_wait(uint32_t(esp_timer_get_time() - w0), woke);
    } else {
        SDL_Delay(1);
    }
    return true;
}

void Game::loop() {
    while (!quit_ && pass(true)) {
    }
}

void Game::shutdown() {
    if (ready_) ESP_LOGI(kTag, "shutting down");
    if (audio_) audio_->shutdown();
    if (presenter_.window()) SDL_StopTextInput(presenter_.window());
    presenter_.destroy();
    if (sdl_ready_) SDL_Quit();
    sdl_ready_ = false;
    ESP_LOGI(kTag, "exit code %d", exit_code_);
    log::flush();
    log::close();
}

// ---------------------------------------------------------------------------
// --autotest
// ---------------------------------------------------------------------------
void Game::push_key(const std::string &name, const std::vector<std::string> &mods) {
    const SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
    SDL_Keymod mod = SDL_KMOD_NONE;
    for (const auto &m : mods) {
        if (m == "shift") mod |= SDL_KMOD_LSHIFT;
        else if (m == "alt") mod |= SDL_KMOD_LALT;
        else if (m == "ctrl") mod |= SDL_KMOD_LCTRL;
    }
    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.timestamp = SDL_GetTicksNS();
    down.key.windowID = SDL_GetWindowID(presenter_.window());
    down.key.scancode = sc;
    down.key.key = SDL_GetKeyFromScancode(sc, mod, true);
    down.key.mod = mod;
    down.key.down = true;
    SDL_PushEvent(&down);
    // A printable key without Ctrl/Alt also types its character, as the OS
    // sends SDL_EVENT_TEXT_INPUT after the key-down ("key Space" types ' ').
    const SDL_Keycode typed = down.key.key;
    if (!(mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) && typed >= 0x20 && typed <= 0x7e) {
        text_buffers_.emplace_back(1, char(typed));
        SDL_Event t{};
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.windowID = down.key.windowID;
        t.text.text = text_buffers_.back().c_str();
        SDL_PushEvent(&t);
    }
    SDL_Event up = down;
    up.type = SDL_EVENT_KEY_UP;
    up.key.down = false;
    SDL_PushEvent(&up);
}

void Game::push_text(const std::string &text) {
    for (char c : text) {
        // The key-down the OS would send first (it resets DesktopInput's suppression).
        SDL_Event down{};
        down.type = SDL_EVENT_KEY_DOWN;
        down.key.windowID = SDL_GetWindowID(presenter_.window());
        down.key.scancode = SDL_SCANCODE_UNKNOWN;
        down.key.down = true;
        SDL_PushEvent(&down);
        text_buffers_.emplace_back(1, c);
        SDL_Event e{};
        e.type = SDL_EVENT_TEXT_INPUT;
        e.text.windowID = SDL_GetWindowID(presenter_.window());
        e.text.text = text_buffers_.back().c_str();
        SDL_PushEvent(&e);
    }
}

bool Game::autotest_expect(const AutotestStep &step) {
    const std::string &name = step.args[0];
    const std::string &value = step.args.size() > 1 ? step.args[1] : std::string();
    std::string actual;
    bool ok = false;
    if (name == "frontend") {
        actual = runtime_->frontend_open() ? "1" : "0";
        ok = actual == value;
    } else if (name == "system_menu") {
        actual = runtime_->system_menu_open() ? "1" : "0";
        ok = actual == value;
    } else if (name == "mode") {
        actual = std::to_string(int(runtime_->ui() ? runtime_->ui()->mode() : openu5::UiMode::Exploration));
        ok = actual == value;
    } else if (name == "frontend_state") {
        actual = std::to_string(int(runtime_->frontend_state()));
        ok = actual == value;
    } else if (name == "transcript" || name == "no_transcript" || name == "transcript_count") {
        // The console's last 24 blocks; no_transcript passes when VALUE is absent.
        const openu5::UiSession *ui = runtime_->ui();
        const size_t count = ui ? ui->transcript_size() : 0;
        for (size_t i = count > 24 ? count - 24 : 0; i < count; ++i) {
            const openu5::UiTextBlock *block = ui->transcript_at(i);
            if (!block) continue;
            actual += std::string(block->text, block->length);
            actual += " | ";
        }
        if (name == "transcript_count") {
            // VALUE = "N TEXT": TEXT occurs exactly N times.
            const size_t space = value.find(' ');
            const std::string needle = space == std::string::npos ? std::string() : value.substr(space + 1);
            size_t found = 0;
            for (size_t at = needle.empty() ? std::string::npos : actual.find(needle); at != std::string::npos;
                 at = actual.find(needle, at + needle.size()))
                ++found;
            ok = !needle.empty() && std::to_string(found) == value.substr(0, space);
            actual = std::to_string(found) + "x in: " + actual;
        } else {
            ok = (actual.find(value) != std::string::npos) == (name == "transcript");
        }
    } else if (name == "footer") {
        // The title/menu page's footer (or the System Menu's) contains VALUE.
        if (runtime_->frontend_open() || runtime_->system_menu_open()) {
            const openu5::FrontendView v = runtime_->frontend_open() ? runtime_->frontend_view() : runtime_->system_menu_view();
            actual = v.footer ? v.footer : "";
            if (v.subtitle && *v.subtitle) actual += std::string(" / ") + v.subtitle;
        }
        ok = actual.find(value) != std::string::npos;
    } else if (name == "position" || name == "remember_position" || name == "remembered_position") {
        // "<location> <floor> <x> <y>" of the live game (GameState::position).
        const auto &p = runtime_->game().position;
        const std::string now = std::to_string(int(p.map.location)) + " " + std::to_string(int(p.map.floor)) + " " +
                                std::to_string(int(p.xy.x)) + " " + std::to_string(int(p.xy.y));
        if (name == "position") {
            actual = now;
            ok = actual == value;
        } else if (name == "remember_position") {
            remembered_position_ = now;
            actual = now;
            ok = true;
        } else {
            // value 1: back where remember_position was taken; 0: elsewhere.
            actual = now == remembered_position_ ? "1" : "0";
            ok = actual == value;
        }
    } else if (name == "combat") {
        actual = runtime_->command_context().combat ? "1" : "0";
        ok = actual == value;
    } else if (name == "remember_combat_actions" || name == "combat_actions_advanced") {
        // The arena's action counter: Engine::advance() bumps it when a turn ends.
        const uint32_t now = runtime_->combat_state().action_count;
        if (name == "remember_combat_actions") {
            remembered_combat_actions_ = now;
            actual = std::to_string(now);
            ok = true;
        } else {
            actual = now != remembered_combat_actions_ ? "1" : "0";
            ok = actual == value;
        }
    } else if (name == "dungeon") {
        actual = runtime_->dungeon_state().active ? "1" : "0";
        ok = actual == value;
    } else if (name == "location") {
        actual = std::to_string(int(runtime_->game().position.map.location));
        ok = actual == value;
    } else if (name == "panel_changed") {
        const uint64_t g = panel::generation();
        actual = g != expect_generation_ ? "1" : "0";
        expect_generation_ = g;
        ok = actual == value;
    } else if (name == "audio_frames_min") {
        const uint64_t frames = audio_ ? audio_->frames_delivered() : 0;
        actual = std::to_string(frames);
        ok = frames >= std::stoull(value);
    } else if (name == "interface") {
        actual = interface_mode_name(settings_.interface_mode);
        ok = actual == value;
    } else if (name == "console" || name == "no_console" || name == "console_last") {
        // The console rows as the screen shows them (wrapped at the live text
        // size, the 1988 model's live prompt row included), joined with " | ";
        // console_last is the bottom row exactly.
        const openu5::UiSession *ui = runtime_->ui();
        size_t columns = 0, rows = 0;
        tdeck::world_transcript_geometry(runtime_->device_settings().ui_size,
                                         runtime_->context_bar_for_test().active, columns, rows);
        if (openu5::platform_dos_screen()) {
            columns = tdeck::kDosConsoleColumns;
            rows = tdeck::kDosConsoleRows;
        }
        openu5::UiRenderedLine lines[tdeck::kConsoleMaxRows]{};
        const size_t n = ui ? ui->visible_lines(lines, std::min(rows, tdeck::kConsoleMaxRows), columns) : 0;
        for (size_t i = 0; i < n; ++i) {
            actual += std::string(lines[i].text, lines[i].length);
            if (i + 1 < n) actual += " | ";
        }
        if (name == "console_last") {
            actual = n ? std::string(lines[n - 1].text, lines[n - 1].length) : std::string();
            ok = actual == value;
        } else {
            ok = (actual.find(value) != std::string::npos) == (name == "console");
        }
    } else if (name == "rows" || name == "no_rows") {
        // The transcript's last 48 blocks as console rows, before any of them
        // scrolls out of the console's window: a block starts a row (unless it
        // continues the one before) and each line feed in it ends one; rows
        // joined with " | ". The 1988 shops' text comes laid out by the core
        // (DosConsole: rows of at most 16 cells), so these are the console's rows.
        const openu5::UiSession *ui = runtime_->ui();
        const size_t count = ui ? ui->transcript_size() : 0;
        bool row_start = true;
        for (size_t i = count > 48 ? count - 48 : 0; i < count; ++i) {
            const openu5::UiTextBlock *block = ui->transcript_at(i);
            if (!block) continue;
            if (!row_start && !(block->flags & openu5::UiTextContinuesBefore)) actual += " | ";
            row_start = false;
            for (size_t k = 0; k < block->length; ++k) {
                if (block->text[k] == '\n') {
                    actual += " | ";
                    row_start = true;
                } else {
                    actual += block->text[k];
                    row_start = false;
                }
            }
        }
        ok = (actual.find(value) != std::string::npos) == (name == "rows");
    } else if (name == "roster_cursor") {
        // The roster row in reverse video for the picker's cursor (-1: none).
        actual = std::to_string(int(runtime_->party_highlight_for_test().cursor));
        ok = actual == value;
    } else if (name == "select_banner") {
        actual = runtime_->party_highlight_for_test().select_banner ? "1" : "0";
        ok = actual == value;
    } else if (name == "overlay_panel") {
        // The handheld's list / page panel over the whole right column (the
        // T-Deck's pickers, Enhanced); a 1988 DOS panel is not one (dos_panel).
        const auto view = runtime_->selection_view_for_test();
        actual = view.active && !view.dos ? "1" : "0";
        ok = actual == value;
    } else if (name == "overlay_title") {
        actual = runtime_->selection_view_for_test().title;
        ok = actual == value;
    } else if (name == "dos_panel" || name == "dos_panel_title" || name == "dos_panel_layout" ||
               name == "dos_panel_row" || name == "dos_panel_bar" || name == "dos_panel_arrow") {
        // The 1988 right panel the runtime publishes (DeviceDosPanel): whether
        // there is one, its banner, its layout (box / split), a window row as
        // text ("N TEXT": row N contains TEXT; box glyphs read '+', RUNES.CH
        // cells '*'), the bar's row (-1 none) and the arrow glyph (0 none).
        const auto view = runtime_->selection_view_for_test();
        const tdeck::DeviceDosPanel *dos = view.active ? view.dos : nullptr;
        auto row_text = [&](size_t r) {
            std::string out;
            for (size_t c = 0; c < tdeck::kDosPanelCols; ++c) {
                const uint8_t code = dos->cells[r][c];
                out += (dos->runes[r] >> c) & 1U ? '*' : code < 0x20 || code > 0x7e ? '+' : char(code);
            }
            return out;
        };
        if (name == "dos_panel") {
            actual = dos ? "1" : "0";
            ok = actual == value;
        } else if (name == "dos_panel_title") {
            actual = dos ? dos->banner : "";
            ok = actual == value;
        } else if (name == "dos_panel_layout") {
            actual = !dos ? "none" : dos->layout == tdeck::kDosLayoutRosterAndStatus ? "split" : "box";
            ok = actual == value;
        } else if (name == "dos_panel_bar") {
            // The bar is a run of reverse-video cells; Mix's single marked cell is not one.
            int bar = -1;
            for (size_t r = 0; dos && r < tdeck::kDosPanelRows && bar < 0; ++r)
                if ((dos->inverse[r] & 0x0ff0) == 0x0ff0) bar = int(r);
            actual = std::to_string(bar);
            ok = actual == value;
        } else if (name == "dos_panel_arrow") {
            actual = std::to_string(dos ? int(dos->arrow) : 0);
            ok = actual == value;
        } else {
            const size_t space = value.find(' ');
            const size_t r = size_t(std::atoi(value.substr(0, space).c_str()));
            const std::string needle = space == std::string::npos ? std::string() : value.substr(space + 1);
            actual = dos && r < tdeck::kDosPanelRows ? row_text(r) : std::string("(no DOS panel)");
            ok = dos && !needle.empty() && actual.find(needle) != std::string::npos;
        }
    } else if (name == "shop_panel") {
        // The handheld's shop panel (offers, gold, log) over the right column.
        actual = runtime_->shop_view_for_test().active ? "1" : "0";
        ok = actual == value;
    } else if (name == "right_column") {
        // What the right column SHOWS, read from the presented panel's pixels
        // (not from the views): on the DOS screen screen_box / screen_split
        // (one box, or the frame's roster and status boxes); on the handheld's
        // dos_box / dos_split (the 1988 panel over the boxes, the console under
        // it), tdeck_list / tdeck_shop (the handheld's right-column panels),
        // world (roster and status boxes); or other.
        actual = right_column_signature();
        ok = actual == value;
    } else if (name == "game_screen") {
        // Which game screen the Board composes: dos (320x200) or handheld (320x240).
        actual = board_->dos_layout() ? "dos" : "handheld";
        ok = actual == value;
    } else if (name == "picture") {
        // The panel rows the window shows now, and the shape the window is sized for.
        actual = "320x" + std::to_string(presenter_.picture_height()) + " in 320x" + std::to_string(presenter_.window_shape());
        ok = actual == value;
    } else if (name == "dos_screen") {
        // The DOS screen's own pixels: "<x> <y> <colour>" with colour band,
        // rule, black or ink (white text), or "frame" (every frame pixel there).
        actual = dos_screen_check(value);
        ok = actual == "ok";
    } else if (name == "context_bar") {
        actual = runtime_->context_bar_for_test().active ? "1" : "0";
        ok = actual == value;
    } else if (name == "prompt") {
        actual = runtime_->ui() ? runtime_->ui()->prompt() : "";
        ok = actual.find(value) != std::string::npos;
    } else if (name == "remember_turns" || name == "turns_advanced") {
        // GameState::turns_since_start: a world turn passed since remember_turns.
        const int64_t now = runtime_->game().turns_since_start;
        if (name == "remember_turns") {
            remembered_turns_ = now;
            actual = std::to_string(now);
            ok = true;
        } else {
            actual = now != remembered_turns_ ? "1" : "0";
            ok = actual == value;
        }
    } else if (name == "equipped") {
        // VALUE = "<member> <item>": that member has the item readied.
        int member = -1, item = -1;
        std::sscanf(value.c_str(), "%d %d", &member, &item);
        const auto &party = runtime_->game().party;
        const bool equipped = member >= 0 && member < party.character_count && item >= 0 && item < 48 &&
                              openu5::is_item_equipped(party.characters[member], item);
        actual = equipped ? "1" : "0";
        ok = equipped;
    } else if (name == "menu_line") {
        // Any line of the open title/menu page (or the System Menu's) contains VALUE.
        if (runtime_->frontend_open() || runtime_->system_menu_open()) {
            const openu5::FrontendView v = runtime_->frontend_open() ? runtime_->frontend_view() : runtime_->system_menu_view();
            for (size_t i = 0; i < v.line_count; ++i) actual += std::string(v.lines[i] ? v.lines[i] : "") + " | ";
        }
        ok = actual.find(value) != std::string::npos;
    } else if (name == "window_layout") {
        // The frame as presented, read back from the renderer, against the
        // layout it was drawn from and the panel it showed.
        std::vector<uint32_t> frame;
        int w = 0, h = 0;
        if (!presenter_.read_window(frame, w, h)) {
            actual = std::string("cannot read the window: ") + SDL_GetError();
        } else {
            const std::string diff = check_presented_frame(presenter_.layout(), panel::pixels(), frame.data(), w, h);
            actual = diff.empty() ? describe_layout(presenter_.layout()) : "pixels differ: " + diff;
        }
        ok = actual == value;
    } else if (name == "picture_sampling") {
        // The picture's sampler as of the last frame drawn (a window_layout draws one).
        actual = presenter_.picture_sampling();
        ok = actual == value;
    } else if (name == "window_scale") {
        actual = std::to_string(int(settings_.window_scale));
        ok = actual == value;
    } else if (name == "scaling") {
        actual = scaling_name(settings_.scaling);
        ok = actual == value;
    } else if (name == "fullscreen") {
        actual = presenter_.fullscreen() ? "1" : "0";
        ok = actual == value;
    } else if (name == "windows_json" || name == "no_windows_json") {
        std::ifstream in(std::filesystem::path(paths_.ultima5_dir() + L"\\windows.json"), std::ios::binary);
        actual = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        while (!actual.empty() && (actual.back() == '\n' || actual.back() == '\r')) actual.pop_back();
        ok = (actual.find(value) != std::string::npos) == (name == "windows_json");
    } else {
        actual = "<unknown check>";
    }
    ESP_LOGI(kTag, "AUTOTEST line %d expect %s=%s actual=%s -> %s", step.line, name.c_str(), value.c_str(),
             actual.c_str(), ok ? "PASS" : "FAIL");
    std::fprintf(stderr, "AUTOTEST line %d: expect %s = %s (actual %s) %s\n", step.line, name.c_str(), value.c_str(),
                 actual.c_str(), ok ? "PASS" : "FAIL");
    return ok;
}

void Game::autotest_dump(int line) {
    std::fprintf(stderr, "AUTOTEST line %d dump: frontend=%d system_menu=%d mode=%d request=%d\n", line,
                 runtime_->frontend_open(), runtime_->system_menu_open(),
                 int(runtime_->ui() ? runtime_->ui()->mode() : openu5::UiMode::Exploration),
                 int(runtime_->ui() ? runtime_->ui()->request() : openu5::UiRequestId::None));
    if (runtime_->frontend_open() || runtime_->system_menu_open()) {
        const openu5::FrontendView v = runtime_->frontend_open() ? runtime_->frontend_view() : runtime_->system_menu_view();
        std::fprintf(stderr, "  [%s] %s\n", v.title ? v.title : "", v.subtitle ? v.subtitle : "");
        for (size_t i = 0; i < v.line_count; ++i)
            std::fprintf(stderr, "  %c %s\n", int(i) == v.selected_line ? '>' : ' ', v.lines[i] ? v.lines[i] : "");
        std::fprintf(stderr, "  footer: %s\n", v.footer ? v.footer : "");
        return;
    }
    const openu5::UiSession *ui = runtime_->ui();
    const size_t count = ui ? ui->transcript_size() : 0;
    for (size_t i = count > 12 ? count - 12 : 0; i < count; ++i) {
        const openu5::UiTextBlock *block = ui->transcript_at(i);
        if (block) std::fprintf(stderr, "  | %.*s\n", int(block->length), block->text);
    }
    if (const char *overlay = runtime_->status_overlay(); overlay && *overlay) std::fprintf(stderr, "  overlay: %s\n", overlay);
    // The console's rows as the screen shows them (the live prompt row included).
    size_t columns = 0, rows = 0;
    tdeck::world_transcript_geometry(runtime_->device_settings().ui_size, runtime_->context_bar_for_test().active,
                                     columns, rows);
    openu5::UiRenderedLine lines[tdeck::kConsoleMaxRows]{};
    const size_t n = ui ? ui->visible_lines(lines, std::min(rows, tdeck::kConsoleMaxRows), columns) : 0;
    for (size_t i = n > 8 ? n - 8 : 0; i < n; ++i) std::fprintf(stderr, "  : %.*s\n", int(lines[i].length), lines[i].text);
}

void Game::autotest_key(const char *name, std::vector<std::string> mods) {
    push_key(name, mods);
    pass(false);
}

void Game::autotest_type(const std::string &text) {
    push_text(text);
    pass(false);
}

// The Developer menu's own Teleport page, driven by keys exactly as a player
// drives it (as the T-Deck screenshot harness does): Alt+D, Teleport,
// Destination = its ordinal in the picker, optional X/Y, Teleport, back out.
// Needs the Developer menu (Enhanced mode or --developer).
bool Game::autotest_teleport(const AutotestStep &step) {
#if !defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    (void)step;
    return false; // the Developer menu is not compiled in
#else
    const std::string &kind_name = step.args[0];
    if (kind_name == "grass") {
        // The open grass the fight step uses (a 5x3 patch of tile 5), alone.
        int gx = -1, gy = -1;
        if (!autotest_open_grass(gx, gy)) return false;
        AutotestStep go;
        go.verb = "teleport";
        go.args = {"britannia", std::to_string(gx), std::to_string(gy)};
        return autotest_teleport(go);
    }
    openu5::DebugDestinationKind kind = openu5::DebugDestinationKind::Britannia;
    if (kind_name == "dungeon") kind = openu5::DebugDestinationKind::Dungeon;
    else if (kind_name == "town") kind = openu5::DebugDestinationKind::SmallMap;
    else if (kind_name == "underworld") kind = openu5::DebugDestinationKind::Underworld;
    else if (kind_name != "britannia") return false;
    const bool large = kind == openu5::DebugDestinationKind::Britannia || kind == openu5::DebugDestinationKind::Underworld;
    const int location = large ? 0 : (step.args.size() > 1 ? std::atoi(step.args[1].c_str()) : 0);
    const auto &context = runtime_->command_context_for_test();
    size_t ordinal = size_t(-1);
    for (size_t i = 0; i < openu5::debug_destination_count(context); ++i) {
        const auto d = openu5::debug_destination_at(context, i);
        if (d.error != openu5::Error::None || d.value.kind != kind) continue;
        if (large || d.value.location == location) {
            ordinal = i;
            break;
        }
    }
    if (ordinal == size_t(-1)) return false;
    autotest_key("D", {"alt"});
    autotest_key("Return", {}); // the Teleport category
    autotest_key("Return", {}); // edit Destination
    autotest_type(std::to_string(ordinal));
    autotest_key("Return", {});
    if (large && step.args.size() >= 3) {
        autotest_key("Down", {}); // Floor
        autotest_key("Down", {}); // X
        autotest_key("Return", {});
        autotest_type(step.args[1]);
        autotest_key("Return", {});
        autotest_key("Down", {}); // Y
        autotest_key("Return", {});
        autotest_type(step.args[2]);
        autotest_key("Return", {});
        autotest_key("Down", {}); // Use default entrance
        autotest_key("Down", {}); // Teleport
    } else {
        for (int i = 0; i < 5; ++i) autotest_key("Down", {});
    }
    autotest_key("Return", {}); // Teleport
    autotest_key("Backspace", {});
    autotest_key("Backspace", {});
    for (int i = 0; i < 20; ++i) pass(false);
    ESP_LOGI(kTag, "AUTOTEST teleport %s ordinal=%u", kind_name.c_str(), unsigned(ordinal));
    return true;
#endif
}

// A surface fight, as the T-Deck screenshot harness stages one: open grass
// (a 5x3 patch of tile 5) found on the live map, the party teleported there,
// N enemies of definition DEF placed beside it, then Pass until the arena opens.
bool Game::autotest_open_grass(int &gx, int &gy) {
    const auto &context = runtime_->command_context_for_test();
    const auto map = openu5::get_active_map(context.world, openu5::MapId{openu5::LocationId(0), 0});
    if (map.error != openu5::Error::None) return false;
    gx = gy = -1;
    for (int y = 60; y < 200 && gx < 0; ++y)
        for (int x = 60; x < 200 && gx < 0; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = map.value.tile_at(x + dx, y + dy) == 5;
            if (ok) {
                gx = x;
                gy = y;
            }
        }
    return gx >= 0;
}

bool Game::autotest_fight(const AutotestStep &step) {
    const int def = std::atoi(step.args[0].c_str());
    const int count = step.args.size() > 1 ? std::clamp(std::atoi(step.args[1].c_str()), 1, 5) : 3;
    auto &context = runtime_->command_context_for_test();
    int gx = -1, gy = -1;
    if (!autotest_open_grass(gx, gy)) return false;
    AutotestStep go;
    go.verb = "teleport";
    go.args = {"britannia", std::to_string(gx), std::to_string(gy)};
    if (!autotest_teleport(go)) return false;
    if (!context.outdoor || !context.outdoor->resources) return false;
    int tile = -1;
    const auto *res = context.outdoor->resources;
    for (size_t i = 0; i < res->enemy_count; ++i)
        if (const auto *e = res->enemies[i]; e && e->index == def) tile = e->tile;
    if (tile < 0) return false;
    context.outdoor->enemies.clear();
    const int dx[] = {1, 1, 1, 2, 2}, dy[] = {0, 1, -1, 0, 1};
    const auto &p = runtime_->game().position.xy;
    for (int k = 0; k < count; ++k) {
        openu5::OutdoorEnemy e{};
        e.definition = def;
        e.tile = tile;
        e.x = p.x + dx[k];
        e.y = p.y + dy[k];
        context.outdoor->enemies.push_back(e);
    }
    for (int turn = 0; turn < 8 && !runtime_->command_context().combat; ++turn) {
        autotest_key("Space", {});
        const int64_t end = esp_timer_get_time() + 400000;
        while (esp_timer_get_time() < end && pass(true)) {
        }
    }
    ESP_LOGI(kTag, "AUTOTEST fight def=%d n=%d at %d,%d combat=%d", def, count, gx, gy, runtime_->command_context().combat);
    return runtime_->command_context().combat;
}

// A shop, as a Talk to its keeper opens one: the first town of that shop type
// through the Developer menu's Teleport page, then the live keeper's begin_shop
// (AlphaRuntime::open_shop_for_test). TYPE: blacksmith, tavern, horses, ship,
// reagents, guild, healer, inn.
bool Game::autotest_shop(const AutotestStep &step) {
    static const struct { const char *name; uint8_t type; int town; } kShops[] = {
        {"blacksmith", 0, 2}, {"tavern", 1, 1}, {"horses", 2, 6}, {"ship", 3, 3},
        {"reagents", 4, 1}, {"guild", 5, 8}, {"healer", 6, 5}, {"inn", 7, 2}};
    for (const auto &shop : kShops) {
        if (step.args[0] != shop.name) continue;
        // `shop TYPE TOWN`: another town of that type (the wine tavern, 19).
        const bool town_given = step.args.size() > 1 && !step.args[1].empty() &&
                                step.args[1][0] >= '0' && step.args[1][0] <= '9';
        AutotestStep go;
        go.verb = "teleport";
        go.args = {"town", town_given ? step.args[1] : std::to_string(shop.town)};
        if (!autotest_teleport(go)) return false;
        if (step.args.size() > 1 && step.args[1] == "beside") {
            // Stand beside the keeper and leave the Talk to the script ("t" + the
            // direction this logs): a real Talk, and a state a save can carry.
            const int toward = runtime_->place_beside_keeper_for_test(shop.type);
            for (int i = 0; i < 10; ++i) pass(false);
            static const char *const kKeys[] = {"Up", "Down", "Right", "Left"}; // openu5::Direction order
            ESP_LOGI(kTag, "AUTOTEST shop %s beside town=%d toward=%s", shop.name, shop.town,
                     toward >= 0 && toward < 4 ? kKeys[toward] : "none");
            return toward >= 0;
        }
        const bool opened = runtime_->open_shop_for_test(shop.type);
        for (int i = 0; i < 10; ++i) pass(false);
        ESP_LOGI(kTag, "AUTOTEST shop %s town=%d opened=%d", shop.name, shop.town, opened);
        return opened;
    }
    return false;
}

// `give gold N` / `give reagent SLOT N`: the counts a shop scenario needs (a
// purse for the purchases, a reagent at its 99 cap).
bool Game::autotest_give(const AutotestStep &step) {
    auto number = [](const std::string &text, int &out) {
        if (text.empty()) return false;
        out = 0;
        for (char c : text) {
            if (c < '0' || c > '9') return false;
            out = out * 10 + (c - '0');
        }
        return true;
    };
    int a = 0, b = 0;
    if (step.args[0] == "gold" && step.args.size() == 2 && number(step.args[1], a) && a <= 9999) {
        runtime_->game().gold = uint16_t(a);
    } else if (step.args[0] == "reagent" && step.args.size() == 3 && number(step.args[1], a) && a < 8 &&
               number(step.args[2], b) && b <= 99) {
        runtime_->game().reagent_quantities[a] = b;
    } else {
        std::fprintf(stderr, "AUTOTEST give: expected 'gold N' or 'reagent SLOT N'\n");
        return false;
    }
    for (int i = 0; i < 2; ++i) pass(false);
    return true;
}

// The right column as the presented panel shows it: the rows of white rule and
// band only one layout draws (the Board's own geometry), read from the pixels.
std::string Game::right_column_signature() const {
    const uint16_t *px = panel::pixels();
    auto at = [&](int x, int y) { return px[size_t(y) * size_t(panel::kWidth) + size_t(x)]; };
    auto run = [&](int y, uint16_t colour, int x0, int x1) {
        for (int x = x0; x <= x1; ++x)
            if (at(x, y) != colour) return false;
        return true;
    };
    constexpr uint16_t kWhite = 0xFFFF, kBand = 0x0015;
    // The DOS screen (board_dos_layout.cpp): the map's box rule at x 184, the
    // band beside it, the panel's box rules at x 191 / x 312, the strip at
    // x 313..319 and the console's top rule either side of the arrow's notch
    // (x 240..263; a banner's notch cuts the box's top rule, so it is not read);
    // its two boxes join into one across the bar at y 57..62.
    if (at(184, 100) == kWhite && run(100, kBand, 185, 190) && at(191, 30) == kWhite && at(312, 30) == kWhite &&
        run(30, kBand, 313, 319) && run(87, kWhite, 191, 239) && run(87, kWhite, 264, 319))
        return at(191, 59) == kWhite && at(312, 59) == kWhite ? "screen_box"
               : run(59, kBand, 192, 230) && run(56, kWhite, 191, 230) ? "screen_split" : "screen_other";
    // The 1988 panel: band rows 0..6 left of the banner's notch, the box's top
    // rule at y 7, the band at 81..86 and the console's top rule at y 87.
    if (run(3, kBand, 184, 200) && run(7, kWhite, 183, 200) && run(83, kBand, 184, 230) && run(87, kWhite, 183, 317))
        return run(56, kWhite, 183, 230) && run(59, kBand, 184, 230) && run(63, kWhite, 183, 317) ? "dos_split" : "dos_box";
    if (run(33, kWhite, 183, 317) && run(35, kWhite, 183, 317) && run(124, kWhite, 183, 317) && run(149, kWhite, 183, 317))
        return "tdeck_shop";
    if (run(23, kWhite, 183, 317) && run(43, kWhite, 183, 317) && run(160, kWhite, 183, 317))
        return "tdeck_list";
    if (run(2, kWhite, 183, 317) && run(53, kWhite, 183, 317) && run(54, kBand, 183, 317) && run(56, kWhite, 183, 317) &&
        run(86, kWhite, 183, 317))
        return "world";
    return "other";
}

// expect dos_screen: "frame" compares every pixel the frame owns outside the
// map, the bands' notches, the panel's window and the console with ULTIMA.EXE's
// frame as the record measures it; "<x> <y> <band|rule|black|ink>" one pixel.
std::string Game::dos_screen_check(const std::string &value) const {
    const uint16_t *px = panel::pixels();
    auto at = [&](int x, int y) { return px[size_t(y) * size_t(panel::kWidth) + size_t(x)]; };
    constexpr uint16_t kWhite = 0xFFFF, kBand = 0x0015, kBlack = 0x0000;
    char text[96];
    if (value == "frame") {
        // Rules and bands that nothing covers: the map's box sides, the
        // separator band, the panel's outer rules beside the roster box, the
        // console's top rule right of the arrow notch, the strip, row 24.
        struct Probe { int x, y; uint16_t want; const char *what; };
        const Probe probes[] = {
            {7, 100, kWhite, "map box left rule"},     {184, 100, kWhite, "map box right rule"},
            {3, 100, kBand, "left band"},              {188, 100, kBand, "separator band"},
            {188, 190, kBand, "separator band bottom"}, {20, 188, kBand, "bottom band"},
            {191, 140, kWhite, "console left rule"},   {300, 87, kWhite, "console top rule"},
            {316, 40, kBand, "right strip"},           {191, 30, kWhite, "roster box left rule"},
            {312, 30, kWhite, "roster box right rule"}, {200, 7, kWhite, "panel top rule"},
            {20, 7, kWhite, "map box top rule"},        {20, 184, kWhite, "map box bottom rule"},
            {0, 0, kBlack, "cut top-left corner"},      {319, 0, kBlack, "cut top-right corner"},
            {0, 191, kBlack, "cut bottom-left corner"}, {100, 195, kBlack, "row 24"},
            {300, 195, kBlack, "row 24 under the console"}};
        for (const auto &p : probes)
            if (at(p.x, p.y) != p.want) {
                std::snprintf(text, sizeof(text), "%s at %d,%d is %04x, expected %04x", p.what, p.x, p.y,
                              unsigned(at(p.x, p.y)), unsigned(p.want));
                return text;
            }
        // Below the 200 rows the presenter shows, the panel is black.
        for (int y = 200; y < panel::kHeight; ++y)
            for (int x = 0; x < panel::kWidth; ++x)
                if (at(x, y) != kBlack) {
                    std::snprintf(text, sizeof(text), "panel row %d is not black at x %d", y, x);
                    return text;
                }
        return "ok";
    }
    int x = -1, y = -1;
    char kind[16]{};
    if (std::sscanf(value.c_str(), "%d %d %15s", &x, &y, kind) != 3 || x < 0 || y < 0 || x >= panel::kWidth ||
        y >= panel::kHeight)
        return "bad check: " + value;
    const uint16_t got = at(x, y);
    const std::string k = kind;
    const bool ok = k == "band" ? got == kBand : k == "rule" || k == "ink" ? got == kWhite : k == "black" ? got == kBlack : false;
    if (ok) return "ok";
    std::snprintf(text, sizeof(text), "%d,%d is %04x, not %s", x, y, unsigned(got), kind);
    return text;
}

bool Game::run_autotest() {
    std::ifstream in(std::filesystem::path(options_.autotest_script), std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "AUTOTEST cannot read the script\n");
        autotest_failures_ = 1;
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> errors;
    const auto steps = parse_autotest(text, errors);
    for (const auto &e : errors) {
        std::fprintf(stderr, "AUTOTEST %s\n", e.c_str());
        ++autotest_failures_;
    }
    expect_generation_ = panel::generation();
    for (const auto &step : steps) {
        if (quit_) break;
        if (step.verb == "frames") {
            const int n = std::max(1, std::atoi(step.args[0].c_str()));
            for (int i = 0; i < n && pass(false); ++i) {
            }
        } else if (step.verb == "wait") {
            const int64_t end = esp_timer_get_time() + int64_t(std::atoll(step.args[0].c_str())) * 1000;
            while (esp_timer_get_time() < end && pass(true)) {
            }
        } else if (step.verb == "key") {
            std::vector<std::string> mods(step.args.begin() + 1, step.args.end());
            push_key(step.args[0], mods);
            pass(false);
        } else if (step.verb == "text") {
            push_text(step.args[0]);
            pass(false);
        } else if (step.verb == "expect") {
            if (!autotest_expect(step)) ++autotest_failures_;
        } else if (step.verb == "screenshot") {
            presenter_.save_screenshot(storage::utf8_to_wide(step.args[0]));
        } else if (step.verb == "window_screenshot") {
            if (!presenter_.save_window_screenshot(storage::utf8_to_wide(step.args[0]))) {
                std::fprintf(stderr, "AUTOTEST line %d: window_screenshot failed: %s\n", step.line, SDL_GetError());
                ++autotest_failures_;
            }
        } else if (step.verb == "window_size") {
            // The resize events arrive on the next passes, as a real drag's do.
            presenter_.resize_client(std::atoi(step.args[0].c_str()), std::atoi(step.args[1].c_str()));
            for (int i = 0; i < 3 && pass(false); ++i) {
            }
        } else if (step.verb == "window_state") {
            if (!presenter_.set_maximized(step.args[0] == "maximize")) {
                std::fprintf(stderr, "AUTOTEST line %d: window_state %s failed: %s\n", step.line, step.args[0].c_str(),
                             SDL_GetError());
                ++autotest_failures_;
            }
            for (int i = 0; i < 3 && pass(false); ++i) {
            }
        } else if (step.verb == "dump") {
            autotest_dump(step.line);
        } else if (step.verb == "teleport") {
            if (!autotest_teleport(step)) ++autotest_failures_;
        } else if (step.verb == "fight") {
            if (!autotest_fight(step)) ++autotest_failures_;
        } else if (step.verb == "shop") {
            if (!autotest_shop(step)) ++autotest_failures_;
        } else if (step.verb == "give") {
            if (!autotest_give(step)) ++autotest_failures_;
        } else if (step.verb == "log") {
            ESP_LOGI(kTag, "AUTOTEST %s", step.args[0].c_str());
        } else if (step.verb == "quit") {
            break;
        }
    }
    std::fprintf(stderr, "AUTOTEST done: %d failure(s); %llu frames presented\n", autotest_failures_,
                 (unsigned long long)presenter_.frames_presented());
    return autotest_failures_ == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Command line
// ---------------------------------------------------------------------------
AppOptions parse_options(int argc, wchar_t **argv) {
    AppOptions o;
    for (int i = 0; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&](std::wstring &out) {
            if (i + 1 < argc) {
                out = argv[++i];
                return true;
            }
            o.errors.push_back(storage::wide_to_utf8(a) + " needs a value");
            return false;
        };
        std::wstring value;
        if (a == L"--data-dir") next(o.data_dir);
        else if (a == L"--autotest") next(o.autotest_script);
        else if (a == L"--console") o.console = true;
        else if (a == L"--developer") o.developer = true;
        else if (a == L"--no-audio") o.no_audio = true;
        else if (a == L"--windowed") o.force_windowed = true;
        else if (a == L"--fullscreen") o.force_fullscreen = true;
        else if (a == L"--help" || a == L"-h" || a == L"/?") o.show_help = true;
        else if (a == L"--mode") {
            if (next(value)) {
                o.interface_override = true;
                if (value == L"enhanced") o.interface_mode = InterfaceMode::Enhanced;
                else if (value == L"original") o.interface_mode = InterfaceMode::Original;
                else o.errors.push_back("--mode takes original or enhanced");
            }
        } else if (a == L"--original-screen") {
            if (next(value)) {
                o.screen_override = true;
                if (value == L"dos") o.original_screen = OriginalScreen::Dos;
                else if (value == L"handheld") o.original_screen = OriginalScreen::Handheld;
                else o.errors.push_back("--original-screen takes dos or handheld");
            }
        } else if (a == L"--log-level") {
            if (next(value)) o.log_level = std::clamp(_wtoi(value.c_str()), 0, 5);
        } else {
            o.errors.push_back("unknown option " + storage::wide_to_utf8(a));
        }
    }
    return o;
}

std::string usage_text() {
    return "Ultima V Native for Windows\n\n"
           "  --data-dir <folder>   use <folder> as the data folder (packs, saves, settings)\n"
           "  --mode original|enhanced   start in this interface mode (this run only)\n"
           "  --original-screen dos|handheld   Original's game screen: the 1988 320x200 one\n"
           "                        (the default) or the handheld's 320x240 (this run only)\n"
           "  --fullscreen / --windowed  start fullscreen or in a window (this run only)\n"
           "  --no-audio            no sound output\n"
           "  --developer           allow the Developer menu (Alt+D) in Original mode\n"
           "  --console             also write the log to a console window\n"
           "  --log-level 0..5      log detail (0 none, 3 info, 5 everything)\n"
           "  --autotest <script>   developers: run a test script (the format is in\n"
           "                        native/targets/windows/src/app/autotest.h in the source)\n";
}

int run_application(const AppOptions &options) {
    Game game(options);
    return game.run();
}

} // namespace uv
