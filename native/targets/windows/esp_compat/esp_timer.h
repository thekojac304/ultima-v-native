#pragma once
// Ultima V Native for Windows: esp_timer_get_time(), microseconds since the
// process started, from the monotonic steady clock (QueryPerformanceCounter).
// The shared runtime uses it for presentation timers (scene pacing, quakes,
// map reveals, the endgame clock) and diagnostics -- never as a gameplay RNG.
#include <cstdint>

int64_t esp_timer_get_time();
