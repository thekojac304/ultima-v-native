#pragma once
// Ultima V Native for Windows: where the 320x240 picture goes in the window.
//
// Pure arithmetic (no SDL), shared by the presenter, the window sizing and
// the tests. The renderer's output (the window's client area, or the whole
// screen in fullscreen) is W x H physical pixels; the picture is the virtual
// panel, never resampled except by a whole number in Framed and Integer:
//
//   Framed   (the default) the largest whole-number scale u that fits,
//            u = max(1, min(W div 320, H div 240)), centred, and a thin
//            ring in the game's own chrome around it where there is room:
//            4u black, a 1u white rule (0xFFFF, the Board's kChromeRule) and
//            a 2u blue band (0x0015, kChromeBand) whose outer corners are cut
//            in black like the Board's frame (kCornerCutSmall). Only the two
//            side pillars when the height has no room for the ring; nothing
//            when the width has none (or the output is under 320x240).
//   Integer  ("Whole pixels") the same picture without the ring.
//   Fit      ("Fit window") the largest 4:3 rectangle; the scale may be
//            fractional (the presenter samples it as pixel art).
//
// Everything else is black, as the 1988 game's surround was. Placement is in
// whole output pixels: an odd leftover puts its extra pixel on the right (or
// bottom) margin, so the ring is always symmetric about the picture.
#include <cstdint>
#include <string>

#include "app/desktop_settings.h"

namespace uv {

constexpr int kPictureW = 320, kPictureH = 240;
// The ring, outward from the picture, in picture pixels (times u on screen).
constexpr int kRingGap = 4;  // black: the Board's own 4 px from its glass edge to the play window (hud.h)
constexpr int kRingRule = 1; // white
constexpr int kRingBand = 2; // blue: band outside rule, as the Board's shell frames its menus
constexpr int kRingReach = kRingGap + kRingRule + kRingBand;
// The band's cut corners, per band row from the outside in: the Board's
// kCornerCutSmall {3, 1} on a band two pixels deep.
constexpr int kRingCornerCut[kRingBand] = {2, 1};
constexpr uint16_t kRingBandRgb565 = 0x0015, kRingRuleRgb565 = 0xFFFF;

struct PixelRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    bool operator==(const PixelRect &) const = default;
};

enum class RingForm : uint8_t { None, Pillars, Full };
enum class RingColour : uint8_t { Band, Rule };

struct RingPiece {
    PixelRect rect;
    RingColour colour = RingColour::Band;
};

constexpr int kMaxRingPieces = 10;

struct PresentationLayout {
    int output_w = 0, output_h = 0;
    Scaling scaling = Scaling::Framed;
    /** Output pixels per picture pixel; 0 when Fit's scale is fractional. */
    int scale = 1;
    /** Where the 320x240 picture is drawn (larger than the output only when the output is smaller than 320x240). */
    PixelRect picture;
    RingForm ring = RingForm::None;
    /** The ring as rectangles of one colour each; they never overlap each other or the picture. */
    RingPiece pieces[kMaxRingPieces]{};
    int piece_count = 0;
};

PresentationLayout layout_presentation(int output_w, int output_h, Scaling scaling);

const char *ring_form_name(RingForm form);
/** "<W>x<H> <full|pillars|none|fit> <w>x<h>@<x>,<y>": what --autotest's window_layout check compares. */
std::string describe_layout(const PresentationLayout &layout);

/**
 * Compares a presented frame (w x h pixels, 0xRRGGBB) with the layout and the
 * 320x240 RGB565 panel it showed. Each frame pixel is reduced to RGB565 (its
 * top bits), which undoes any renderer's 5/6-to-8-bit expansion exactly, so
 * the picture, the ring and the black are compared colour for colour. A
 * fractional Fit picture may show any of the 3x3 texels around the nearest
 * one. Returns an empty string when they agree, else the first difference.
 */
std::string check_presented_frame(const PresentationLayout &layout, const uint16_t *panel, const uint32_t *frame,
                                  int w, int h);

} // namespace uv
