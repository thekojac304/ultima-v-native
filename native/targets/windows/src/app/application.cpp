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
#include "app/presenter.h"
#include "app/sdl_audio_backend.h"
#include "asset_pack.h"
#include "build_info.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
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

/** A missing or mismatched pack: explain, and offer to open the data folder. */
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
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Open data folder"},
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
// interleave save generations.
// ---------------------------------------------------------------------------
class InstanceLock {
  public:
    explicit InstanceLock(const std::wstring &data_dir) {
        std::wstring name = L"Local\\UltimaVNative-";
        size_t h = std::hash<std::wstring>{}(data_dir);
        name += std::to_wstring(h);
        handle_ = CreateMutexW(nullptr, TRUE, name.c_str());
        owned_ = handle_ && GetLastError() != ERROR_ALREADY_EXISTS;
    }
    ~InstanceLock() {
        if (handle_) {
            if (owned_) ReleaseMutex(handle_);
            CloseHandle(handle_);
        }
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
    void apply_command(const DesktopCommand &command);
    InputContext input_context() const;
    void sync_mode();
    void update_title();
    void save_settings();
    void take_screenshot();
    bool run_autotest();
    bool autotest_expect(const AutotestStep &step);
    void autotest_dump(int line);
    void push_key(const std::string &name, const std::vector<std::string> &mods);
    void push_text(const std::string &text);

    AppOptions options_;
    DataPaths paths_{};
    DesktopSettings settings_{};
    std::unique_ptr<InstanceLock> lock_;
    Presenter presenter_;
    DesktopInput input_;
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
    if (options_.interface_override) settings_.interface_mode = options_.interface_mode;
    if (options_.force_fullscreen) settings_.fullscreen = true;
    if (options_.force_windowed) settings_.fullscreen = false;
    applied_mode_ = settings_.interface_mode;
    input_.set_mode(settings_.interface_mode);

    // --- Window --------------------------------------------------------------------
    if (!presenter_.create(build::kProductName, settings_)) {
        show_error(nullptr, "Ultima V Native", "The game window could not be created.\n\n" + presenter_.error());
        exit_code_ = 5;
        return false;
    }
    update_title();

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
    input_.set_mode(applied_mode_);
    update_title();
    ESP_LOGI(kTag, "INTERFACE_MODE %s", interface_mode_name(applied_mode_));
}

void Game::update_title() {
    if (!presenter_.window()) return;
    const std::string title =
        std::string(build::kProductName) + " - " + interface_mode_name(settings_.interface_mode) + " mode";
    SDL_SetWindowTitle(presenter_.window(), title.c_str());
}

void Game::save_settings() {
    if (!save_desktop_settings(paths_.ultima5_dir() + L"\\windows.json", settings_))
        ESP_LOGW(kTag, "windows.json could not be written");
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
            settings_.fullscreen = false;
            presenter_.set_fullscreen(false);
        }
        presenter_.set_window_scale(settings_.window_scale);
        save_settings();
        break;
    }
    case DesktopCommand::Kind::Screenshot: take_screenshot(); break;
    case DesktopCommand::Kind::None: break;
    }
}

void Game::handle_event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_QUIT: quit_ = true; return;
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
    } else if (name == "transcript") {
        const openu5::UiSession *ui = runtime_->ui();
        const size_t count = ui ? ui->transcript_size() : 0;
        for (size_t i = count > 24 ? count - 24 : 0; i < count; ++i) {
            const openu5::UiTextBlock *block = ui->transcript_at(i);
            if (!block) continue;
            actual += std::string(block->text, block->length);
            actual += " | ";
        }
        ok = actual.find(value) != std::string::npos;
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
           "  --mode original|enhanced   start in this interface mode (saved)\n"
           "  --fullscreen / --windowed  start fullscreen or in a window\n"
           "  --no-audio            no sound output\n"
           "  --developer           allow the Developer menu (Alt+D) in Original mode\n"
           "  --console             also write the log to a console window\n"
           "  --log-level 0..5      log detail (0 none, 3 info, 5 everything)\n"
           "  --autotest <script>   run a test script (see README-Windows.txt)\n";
}

int run_application(const AppOptions &options) {
    Game game(options);
    return game.run();
}

} // namespace uv
