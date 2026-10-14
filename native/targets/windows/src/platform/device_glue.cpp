// Ultima V Native for Windows: the one symbol of the T-Deck input driver
// (native/targets/tdeck/main/tdeck_input.cpp, not built here) the shared
// runtime names: the RawInputKind name its input log lines print.
#include "input_events.h"

namespace tdeck {

const char *raw_input_name(RawInputKind kind) {
    switch (kind) {
    case RawInputKind::Keyboard: return "keyboard";
    case RawInputKind::KeyboardResynchronized: return "keyboard-resynchronized";
    case RawInputKind::TrackballUp: return "trackball-up";
    case RawInputKind::TrackballDown: return "trackball-down";
    case RawInputKind::TrackballLeft: return "trackball-left";
    case RawInputKind::TrackballRight: return "trackball-right";
    case RawInputKind::TrackballClick: return "trackball-click";
    case RawInputKind::None: return "none";
    }
    return "unknown";
}

} // namespace tdeck
