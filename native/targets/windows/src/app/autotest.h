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
//                           transcript TEXT / no_transcript TEXT (the console's
//                             last blocks contain / do not contain TEXT),
//                           transcript_count "N TEXT" (TEXT occurs exactly N
//                             times in those blocks),
//                           footer TEXT (the title or System Menu page's footer
//                             or subtitle contains TEXT),
//                           interface Original|Enhanced,
//                           position "<location> <floor> <x> <y>",
//                           remember_position (any value: store the position),
//                           remembered_position 1|0 (back where stored / not),
//                           combat 0|1, dungeon 0|1, location <id>,
//                           remember_combat_actions (any value: store the
//                             arena's action counter), combat_actions_advanced
//                             1|0 (an arena turn ended since / none did),
//                           panel_changed 0|1 (since the previous expect),
//                           audio_frames_min N,
//                           console TEXT / no_console TEXT (the console rows
//                             as the screen shows them, joined " | ", contain /
//                             do not contain TEXT), console_last TEXT (the
//                             bottom row is exactly TEXT),
//                           roster_cursor N (the picker's reverse-video roster
//                             row, -1 none), select_banner 0|1,
//                           overlay_panel 0|1 (a list/page panel covers the
//                             right column), overlay_title TEXT,
//                           context_bar 0|1, prompt TEXT (the open prompt
//                             contains TEXT),
//                           remember_turns (any value) / turns_advanced 1|0,
//                           equipped "<member> <item>"
//   screenshot PATH       write the panel as a BMP
//   log TEXT              write TEXT to the log
//   dump                  print the title/menu page or the console's last lines
//   teleport KIND [LOC | X Y]  through the Developer menu's Teleport page (needs
//                         Enhanced or --developer): britannia X Y, town LOC,
//                         dungeon LOC (33 Deceit ... 40), underworld
//   fight DEF [N]         a surface fight: open grass, N enemies of definition
//                         DEF beside the party, Pass until the arena opens
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
