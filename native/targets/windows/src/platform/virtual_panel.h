#pragma once
// Ultima V Native for Windows: the virtual ST7789 panel.
//
// The shared presentation (native/targets/tdeck/main/tdeck_board.cpp) composes
// every screen -- title, menus, the play frame, transcript, shops, the ending --
// by streaming ST7789 commands over SPI: a window (CASET 0x2A / RASET 0x2B),
// RAMWR 0x2C, then big-endian RGB565 pixels that fill the window row by row.
// It keeps no framebuffer of its own and redraws only what changed, relying
// on the panel to keep its contents. This module is that panel: it implements
// the SPI/GPIO/LEDC driver calls the Board makes (esp_compat/driver/*.h) and
// decodes the stream into a persistent 320x240 RGB565 memory, which the SDL
// presenter shows. Board code is compiled unchanged, so the Windows screens
// are pixel-for-pixel the T-Deck's.
//
// Only CASET/RASET/RAMWR and pixel data change the memory. The rotation and
// colour-mode commands the Board sends at start-up (MADCTL 0x36 = MX|MV|RGB,
// COLMOD 0x3A = 16 bit, INVON 0x21) describe the physical panel; their effect
// is that Board coordinates are landscape pixels and values are true RGB565,
// which is what this memory stores.
#include <cstddef>
#include <cstdint>

namespace uv::panel {

constexpr int kWidth = 320;
constexpr int kHeight = 240;

/** The panel memory, kWidth x kHeight RGB565, row-major (pitch kWidth * 2). */
const uint16_t *pixels();
/** Incremented by every pixel write; the presenter uploads when it moved. */
uint64_t generation();
/** The backlight duty the Board last set, 0..1023 (1023 = full). */
uint32_t backlight_duty();
/** Fills the memory (power-on content) and resets the decoder; tests and start-up. */
void reset(uint16_t fill = 0x0000);

struct Stats {
    uint64_t transactions = 0;
    uint64_t pixel_bytes = 0;
    uint64_t windows = 0;      // RAMWR commands
    uint64_t malformed = 0;    // odd pixel bytes, writes outside the panel, oversized transactions
};
const Stats &stats();

} // namespace uv::panel
