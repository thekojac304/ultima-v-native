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
    static void on_profile_change(void *context, DesktopProfile::Change what);
    void install_profile();
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
    std::unique_ptr<InstanceLock> lock_;
    Presenter presenter_;
    DesktopInput input_;
    DesktopProfile profile_;
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
    presenter_.show();
    presenter_.present(true);
    SDL_StartTextInput(presenter_.window());
    heartbeat_us_ = esp_timer_get_time() + 5000000;
    ready_ = true;
    ESP_LOGI(kTag, "ready: interface=%s window=%ux%s fullscreen=%d", interface_mode_name(settings_.interface_mode),
             unsigned(settings_.window_scale), scaling_name(settings_.scaling), settings_.fullscreen);
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
    update_title();
    save_settings();
    ESP_LOGI(kTag, "INTERFACE_MODE %s", interface_mode_name(applied_mode_));
}

void Game::install_profile() {
    DesktopProfile::Hooks hooks;
    hooks.settings = &settings_;
    hooks.context = this;
    hooks.changed = &Game::on_profile_change;
    profile_.install(hooks);
}

// A desktop Settings row (Interface, Display, Scaling) changed settings_.
void Game::on_profile_change(void *context, DesktopProfile::Change what) {
    auto *self = static_cast<Game *>(context);
    switch (what) {
    case DesktopProfile::Change::Interface: self->sync_mode(); return; // persists too
    case DesktopProfile::Change::Display:
        self->fullscreen_overridden_ = false;
        self->presenter_.set_fullscreen(self->settings_.fullscreen);
        if (!self->settings_.fullscreen) self->presenter_.set_window_scale(self->settings_.window_scale);
        break;
    case DesktopProfile::Change::Scaling: self->presenter_.set_scaling(self->settings_.scaling); break;
    }
    self->save_settings();
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
        const int delta = command.kind == DesktopCommand::Kind::WindowScaleUp ? 1 : -1;
        settings_.window_scale =
            uint8_t(std::clamp(int(settings_.window_scale) + delta, int(kMinWindowScale), int(kMaxWindowScale)));
        if (settings_.fullscreen) {
            fullscreen_overridden_ = false;
            settings_.fullscreen = false;
            presenter_.set_fullscreen(false);
        }
        presenter_.set_window_scale(settings_.window_scale);
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
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_RENDER_TARGETS_RESET:
    case SDL_EVENT_RENDER_DEVICE_RESET: presenter_.invalidate(); return;
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
bool Game::autotest_fight(const AutotestStep &step) {
    const int def = std::atoi(step.args[0].c_str());
    const int count = step.args.size() > 1 ? std::clamp(std::atoi(step.args[1].c_str()), 1, 5) : 3;
    auto &context = runtime_->command_context_for_test();
    const auto map = openu5::get_active_map(context.world, openu5::MapId{openu5::LocationId(0), 0});
    if (map.error != openu5::Error::None) return false;
    int gx = -1, gy = -1;
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
    if (gx < 0) return false;
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
        } else if (step.verb == "dump") {
            autotest_dump(step.line);
        } else if (step.verb == "teleport") {
            if (!autotest_teleport(step)) ++autotest_failures_;
        } else if (step.verb == "fight") {
            if (!autotest_fight(step)) ++autotest_failures_;
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
