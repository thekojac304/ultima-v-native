#pragma once
// Ultima V Native for Windows: the application -- the desktop counterpart of
// the T-Deck's app_main (native/targets/tdeck/main/main.cpp).
//
// Boot: data directory -> log -> desktop settings -> SDL -> window -> the
// shared Board on the virtual panel -> the two resource packs (with the
// T-Deck's identity gate) -> AlphaRuntime::initialize() -> the optional audio
// pack -> first frame. Loop: SDL events through DesktopInput into
// AlphaRuntime::handle_action(), AlphaRuntime::render() into the panel, the
// presenter shows it, and the loop waits as long as the runtime allows
// (loop_wait_ticks(), the device's own pacing rule).
//
// --autotest <script> runs a deterministic script of synthetic SDL key events
// and state checks through that same loop (tests/ uses it with the offscreen
// video driver); the exit code is the number of failed checks.
#include <cstdint>
#include <string>
#include <vector>

#include "app/desktop_settings.h"

namespace uv {

struct AppOptions {
    std::wstring data_dir;          // --data-dir
    std::wstring autotest_script;   // --autotest
    bool console = false;           // --console: log to a console window too
    bool developer = false;         // --developer: Alt+D in Original mode as well
    bool no_audio = false;          // --no-audio
    bool force_windowed = false;    // --windowed
    bool force_fullscreen = false;  // --fullscreen
    bool interface_override = false;
    InterfaceMode interface_mode = InterfaceMode::Original; // --mode original|enhanced
    int log_level = -1;             // --log-level 0..5 (ESP_LOG_*); -1 = default
    bool show_help = false;
    std::vector<std::string> errors;
};

/** Parses the command line (argv[0] excluded). Unknown options are reported in errors. */
AppOptions parse_options(int argc, wchar_t **argv);
std::string usage_text();

/** Runs the game; returns the process exit code. */
int run_application(const AppOptions &options);

} // namespace uv
