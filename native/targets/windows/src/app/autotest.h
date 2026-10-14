#pragma once
// Ultima V Native for Windows: the --autotest script runner.
//
// A script is one command per line ('#' starts a comment):
//   frames N              run N loop passes
//   wait MS               run the loop for MS milliseconds of real time
//   key NAME [mods...]    a key press and release; NAME is an SDL scancode
//                         name ("Return", "Escape", "Up", "F10", "A", ...);
//                         mods: shift alt ctrl. Synthetic SDL events, so the
//                         key travels DesktopInput -> AlphaRuntime like a real one
//   text STRING           SDL text input, one character at a time
//   expect NAME VALUE     check a runtime fact; a failed check is counted:
//                           frontend 0|1, system_menu 0|1, mode <UiMode number>,
//                           frontend_state <FrontendState number>,
//                           transcript <substring of the console's last lines>,
//                           panel_changed 0|1 (since the previous expect),
//                           audio_frames_min N
//   screenshot PATH       write the panel as a BMP
//   log TEXT              write TEXT to the log
//   dump                  print the title/menu page or the console's last lines
//   quit                  stop (the script's end stops too)
#include <cstdint>
#include <string>
#include <vector>

namespace uv {

struct AutotestStep {
    std::string verb;
    std::vector<std::string> args;
    int line = 0;
};

/** Parses a script; malformed lines are reported in `errors` (and skipped). */
std::vector<AutotestStep> parse_autotest(const std::string &text, std::vector<std::string> &errors);

} // namespace uv
