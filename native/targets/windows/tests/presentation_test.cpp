// Ultima V Native for Windows -- the presentation (app/presentation_layout.h).
//
// The picture's placement and the Classic Framed ring, as pure arithmetic:
// exact layouts for the common desktop and window sizes, then every output
// size on a dense grid (and every size up to 1100x850) against the rules --
// whole-number scale, centring with the odd pixel on the right/bottom, the
// ring's three forms and their thresholds, nothing outside the output,
// nothing over the picture, no overlaps, mirror symmetry -- then the frame
// checker the --autotest window_layout step uses, against frames drawn here
// from the layout (nearest, and a fractional Fit blended as Direct3D 11's
// pixel-art sampling does; and against deliberately wrong ones), and the
// Settings page's Scaling row.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "app/desktop_profile.h"
#include "app/presentation_layout.h"
#include "openu5/platform_profile.h"
#include "test_check.h"

using uv::layout_presentation;
using uv::PixelRect;
using uv::PresentationLayout;
using uv::RingColour;
using uv::RingForm;
using uv::Scaling;
using uvtest::check;
using uvtest::format;

namespace {

bool overlaps(const PixelRect &a, const PixelRect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool inside(const PixelRect &r, int w, int h) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= w && r.y + r.h <= h; }

std::string rect_text(const PixelRect &r) { return format("(%d,%d %dx%d)", r.x, r.y, r.w, r.h); }

// --- 1. Exact layouts ---------------------------------------------------------
void exact_sizes() {
    struct Row {
        int w, h;
        const char *framed, *integer, *fit;
    };
    // Worked out by hand from the rules in presentation_layout.h.
    const Row rows[] = {
        {640, 480, "640x480 none 640x480@0,0", "640x480 none 640x480@0,0", "640x480 fit 640x480@0,0"},
        {1008, 768, "1008x768 full 960x720@24,24", "1008x768 none 960x720@24,24", "1008x768 fit 1008x756@0,6"},
        {1280, 720, "1280x720 pillars 960x720@160,0", "1280x720 none 960x720@160,0", "1280x720 fit 960x720@160,0"},
        {1366, 768, "1366x768 full 960x720@203,24", "1366x768 none 960x720@203,24", "1366x768 fit 1024x768@171,0"},
        {1600, 900, "1600x900 full 960x720@320,90", "1600x900 none 960x720@320,90", "1600x900 fit 1200x900@200,0"},
        {1920, 1080, "1920x1080 full 1280x960@320,60", "1920x1080 none 1280x960@320,60", "1920x1080 fit 1440x1080@240,0"},
        {1920, 1200, "1920x1200 pillars 1600x1200@160,0", "1920x1200 none 1600x1200@160,0", "1920x1200 fit 1600x1200@160,0"},
        {2560, 1440, "2560x1440 pillars 1920x1440@320,0", "2560x1440 none 1920x1440@320,0", "2560x1440 fit 1920x1440@320,0"},
        {2560, 1600, "2560x1600 full 1920x1440@320,80", "2560x1600 none 1920x1440@320,80", "2560x1600 fit 2133x1600@213,0"},
        {3440, 1440, "3440x1440 pillars 1920x1440@760,0", "3440x1440 none 1920x1440@760,0", "3440x1440 fit 1920x1440@760,0"},
        {3840, 2160, "3840x2160 pillars 2880x2160@480,0", "3840x2160 none 2880x2160@480,0", "3840x2160 fit 2880x2160@480,0"},
        {1024, 768, "1024x768 full 960x720@32,24", "1024x768 none 960x720@32,24", "1024x768 fit 1024x768@0,0"},
        {1280, 1024, "1280x1024 none 1280x960@0,32", "1280x1024 none 1280x960@0,32", "1280x1024 fit 1280x960@0,32"},
        {336, 256, "336x256 full 320x240@8,8", "336x256 none 320x240@8,8", "336x256 fit 336x252@0,2"},
        {672, 512, "672x512 full 640x480@16,16", "672x512 none 640x480@16,16", "672x512 fit 672x504@0,4"},
        {1367, 769, "1367x769 full 960x720@203,24", "1367x769 none 960x720@203,24", "1367x769 fit 1025x769@171,0"},
        {320, 240, "320x240 none 320x240@0,0", "320x240 none 320x240@0,0", "320x240 fit 320x240@0,0"},
        {200, 150, "200x150 none 320x240@-60,-45", "200x150 none 320x240@-60,-45", "200x150 fit 200x150@0,0"},
        {0, 0, "0x0 none 320x240@-160,-120", "0x0 none 320x240@-160,-120", "0x0 fit 0x0@0,0"},
    };
    for (const Row &r : rows) {
        const std::string framed = describe_layout(layout_presentation(r.w, r.h, Scaling::Framed));
        const std::string integer = describe_layout(layout_presentation(r.w, r.h, Scaling::Integer));
        const std::string fit = describe_layout(layout_presentation(r.w, r.h, Scaling::Fit));
        check(framed == r.framed, format("L1 %dx%d Framed: %s", r.w, r.h, r.framed), "got " + framed);
        check(integer == r.integer, format("L2 %dx%d Whole pixels: %s", r.w, r.h, r.integer), "got " + integer);
        check(fit == r.fit, format("L3 %dx%d Fit: %s", r.w, r.h, r.fit), "got " + fit);
    }
    // The research's worked 1080p ring: band x292-299, rule x300-303, gap
    // x304-319; band y32-39, rule y40-43, gap y44-59.
    const PresentationLayout l = layout_presentation(1920, 1080, Scaling::Framed);
    bool band_left = false, rule_left = false, band_top = false, rule_top = false;
    for (int i = 0; i < l.piece_count; ++i) {
        const PixelRect &r = l.pieces[i].rect;
        if (l.pieces[i].colour == RingColour::Band && r == PixelRect{292, 40, 8, 1000}) band_left = true;
        if (l.pieces[i].colour == RingColour::Rule && r == PixelRect{300, 44, 4, 992}) rule_left = true;
        if (l.pieces[i].colour == RingColour::Band && r == PixelRect{300, 32, 1320, 4}) band_top = true;
        if (l.pieces[i].colour == RingColour::Rule && r == PixelRect{300, 40, 1320, 4}) rule_top = true;
    }
    check(l.piece_count == 10 && band_left && rule_left && band_top && rule_top,
          "L4 1920x1080 ring: band x292-299 / y32-39, rule x300-303 / y40-43, 16 px black gap to the picture",
          format("pieces=%d band_left=%d rule_left=%d band_top=%d rule_top=%d", l.piece_count, band_left, rule_left,
                 band_top, rule_top));
    // 2560x1440 pillars: bands x278-289 and x2270-2281 (the research note's
    // "2230-2241" was a slip: the right band mirrors the left one about the picture).
    const PresentationLayout p = layout_presentation(2560, 1440, Scaling::Framed);
    check(p.piece_count == 4 && p.pieces[0].rect == PixelRect{278, 0, 12, 1440} &&
              p.pieces[1].rect == PixelRect{290, 0, 6, 1440} && p.pieces[2].rect == PixelRect{2264, 0, 6, 1440} &&
              p.pieces[3].rect == PixelRect{2270, 0, 12, 1440},
          "L5 2560x1440 pillars: band x278-289, rule x290-295 | picture x320-2239 | rule x2264-2269, band x2270-2281",
          format("%d pieces, first %s, last %s", p.piece_count, rect_text(p.pieces[0].rect).c_str(),
                 rect_text(p.pieces[p.piece_count ? p.piece_count - 1 : 0].rect).c_str()));
    // The cut corners: the top-left band rows start 2u and 1u in from the band's outer edge.
    check(l.pieces[4].rect == PixelRect{300, 32, 1320, 4} && l.pieces[6].rect == PixelRect{296, 36, 1328, 4},
          "L6 cut corners at u=4: the outer band row starts at x300 (cut 8 px), the inner one at x296 (cut 4 px)",
          rect_text(l.pieces[4].rect) + " " + rect_text(l.pieces[6].rect));
}

// --- 2. Every size against the rules ---------------------------------------------
struct Failure {
    int count = 0;
    std::string first;
    void note(const std::string &what) {
        if (count++ == 0) first = what;
    }
};

void rules_for(int W, int H, Failure &f) {
    const PresentationLayout fr = layout_presentation(W, H, Scaling::Framed);
    const PresentationLayout in = layout_presentation(W, H, Scaling::Integer);
    const PresentationLayout fit = layout_presentation(W, H, Scaling::Fit);
    const std::string at = format("%dx%d: ", W, H);
    const int u = std::max(1, std::min(W / 320, H / 240));
    // Whole-number scale, never reduced for the ring.
    if (fr.scale != u || in.scale != u) f.note(at + format("scale %d/%d, expected %d", fr.scale, in.scale, u));
    if (fr.picture.w != 320 * u || fr.picture.h != 240 * u) f.note(at + "picture is not 320u x 240u");
    if (!(fr.picture == in.picture)) f.note(at + "Framed and Whole pixels place the picture differently");
    // Centred; the odd pixel goes right / down.
    const int left = fr.picture.x, right = W - fr.picture.x - fr.picture.w;
    const int top = fr.picture.y, bottom = H - fr.picture.y - fr.picture.h;
    if (right - left < 0 || right - left > 1 || bottom - top < 0 || bottom - top > 1)
        f.note(at + format("margins L%d R%d T%d B%d are not centred", left, right, top, bottom));
    if (W >= 320 && H >= 240 && !inside(fr.picture, W, H)) f.note(at + "picture outside the output");
    // The ring's form and thresholds (both sides of an axis or neither).
    const int reach = 7 * u;
    const RingForm want = left < reach || top < 0 ? RingForm::None : top < reach ? RingForm::Pillars : RingForm::Full;
    if (fr.ring != want) f.note(at + format("ring %s, expected %s", uv::ring_form_name(fr.ring), uv::ring_form_name(want)));
    const int want_pieces = want == RingForm::Full ? 10 : want == RingForm::Pillars ? 4 : 0;
    if (fr.piece_count != want_pieces) f.note(at + format("%d ring pieces, expected %d", fr.piece_count, want_pieces));
    if (in.ring != RingForm::None || in.piece_count != 0 || fit.piece_count != 0) f.note(at + "a ring outside Framed");
    long long ring_area = 0;
    for (int i = 0; i < fr.piece_count; ++i) {
        const PixelRect &r = fr.pieces[i].rect;
        if (r.w <= 0 || r.h <= 0) f.note(at + "empty ring piece " + rect_text(r));
        if (!inside(r, W, H)) f.note(at + "ring piece outside the output " + rect_text(r));
        if (overlaps(r, fr.picture)) f.note(at + "ring piece over the picture " + rect_text(r));
        for (int j = i + 1; j < fr.piece_count; ++j)
            if (overlaps(r, fr.pieces[j].rect)) f.note(at + "ring pieces overlap " + rect_text(r) + " " + rect_text(fr.pieces[j].rect));
        // Mirror symmetry about the picture's centre lines.
        const PixelRect mx{2 * fr.picture.x + fr.picture.w - r.x - r.w, r.y, r.w, r.h};
        bool mirrored = false;
        for (int j = 0; j < fr.piece_count; ++j)
            mirrored = mirrored || (fr.pieces[j].rect == mx && fr.pieces[j].colour == fr.pieces[i].colour);
        if (!mirrored) f.note(at + "no left/right mirror for " + rect_text(r));
        if (want == RingForm::Full) {
            const PixelRect my{r.x, 2 * fr.picture.y + fr.picture.h - r.y - r.h, r.w, r.h};
            bool flipped = false;
            for (int j = 0; j < fr.piece_count; ++j)
                flipped = flipped || (fr.pieces[j].rect == my && fr.pieces[j].colour == fr.pieces[i].colour);
            if (!flipped) f.note(at + "no top/bottom mirror for " + rect_text(r));
        }
        // Each piece's distance from the picture: rule 4u..5u, band 5u..7u.
        const bool rule = fr.pieces[i].colour == RingColour::Rule;
        const int lo = rule ? 4 * u : 5 * u, hi = rule ? 5 * u : 7 * u;
        const int dx = r.x + r.w <= fr.picture.x ? fr.picture.x - r.x : r.x >= fr.picture.x + fr.picture.w ? r.x + r.w - (fr.picture.x + fr.picture.w) : 0;
        const int dy = r.y + r.h <= fr.picture.y ? fr.picture.y - r.y : r.y >= fr.picture.y + fr.picture.h ? r.y + r.h - (fr.picture.y + fr.picture.h) : 0;
        const int d = std::max(dx, dy);
        if (d <= lo || d > hi) f.note(at + format("%s piece %s reaches %d px from the picture (expected %d..%d)", rule ? "rule" : "band", rect_text(r).c_str(), d, lo + 1, hi));
        ring_area += 1LL * r.w * r.h;
    }
    // Full ring area: rules 2*330u*u + 2*248u*u, band 2*(330+332)u*u + 2*2u*250u.
    if (want == RingForm::Full && ring_area != (2LL * 330 + 2 * 248 + 2 * (330 + 332) + 2 * 2 * 250) * u * u)
        f.note(at + format("full ring area %lld", ring_area));
    if (want == RingForm::Pillars && ring_area != 6LL * u * H) f.note(at + format("pillar area %lld", ring_area));
    // Fit: the largest 4:3 rectangle, centred, inside the output.
    const PixelRect &q = fit.picture;
    if (W > 0 && H > 0) {
        if (!(q.w == W || q.h == H)) f.note(at + "Fit touches neither pair of edges");
        if (!inside(q, W, H)) f.note(at + "Fit picture outside the output");
        if (std::abs(1LL * q.w * 240 - 1LL * q.h * 320) >= 320) f.note(at + format("Fit %dx%d is not 4:3", q.w, q.h));
        const int fl = q.x, frr = W - q.x - q.w, ft = q.y, fb = H - q.y - q.h;
        if (frr - fl < 0 || frr - fl > 1 || fb - ft < 0 || fb - ft > 1) f.note(at + "Fit is not centred");
        const bool exact = q.w % 320 == 0 && q.w / 320 * 240 == q.h;
        if ((fit.scale != 0) != exact || (exact && fit.scale != q.w / 320)) f.note(at + format("Fit scale %d", fit.scale));
    }
}

void every_size() {
    Failure dense, grid;
    int n = 0;
    for (int W = 0; W <= 1100; ++W)
        for (int H = 0; H <= 850; ++H, ++n) rules_for(W, H, dense);
    check(dense.count == 0, format("R1 every output size 0..1100 x 0..850 (%d sizes, three scalings) obeys the layout rules", n),
          format("%d failures, first: %s", dense.count, dense.first.c_str()));
    n = 0;
    for (int W = 1100; W <= 7680; W += 13)
        for (int H = 850; H <= 4320; H += 11, ++n) rules_for(W, H, grid);
    check(grid.count == 0, format("R2 a grid of larger outputs up to 7680x4320 (%d sizes) obeys the layout rules", n),
          format("%d failures, first: %s", grid.count, grid.first.c_str()));
}

// --- 3. The frame checker -------------------------------------------------------------
std::vector<uint16_t> test_panel() {
    std::vector<uint16_t> panel(320 * 240);
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) panel[size_t(y) * 320 + size_t(x)] = uint16_t((x * 2654435761u + y * 40503u) >> 7);
    panel[0] = 0x0015; // the band's own blue in the picture
    return panel;
}

uint32_t expand(uint16_t p) { // the bit-replicating 5/6 -> 8 expansion SDL's blitter uses
    const uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

uint32_t unorm(uint16_t p) { // a GPU's UNORM conversion, rounded
    const uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
    return (((r * 255 + 15) / 31) << 16) | (((g * 255 + 31) / 63) << 8) | ((b * 255 + 15) / 31);
}

std::vector<uint32_t> draw(const PresentationLayout &l, const std::vector<uint16_t> &panel, uint32_t (*conv)(uint16_t)) {
    std::vector<uint32_t> frame(size_t(l.output_w) * size_t(l.output_h), 0);
    for (int i = 0; i < l.piece_count; ++i) {
        const PixelRect &r = l.pieces[i].rect;
        const uint32_t c = conv(l.pieces[i].colour == RingColour::Rule ? uv::kRingRuleRgb565 : uv::kRingBandRgb565);
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) frame[size_t(y) * size_t(l.output_w) + size_t(x)] = c;
    }
    const PixelRect &p = l.picture;
    for (int y = std::max(0, p.y); y < std::min(l.output_h, p.y + p.h); ++y)
        for (int x = std::max(0, p.x); x < std::min(l.output_w, p.x + p.w); ++x) {
            const int sx = l.scale ? (x - p.x) / l.scale : int(int64_t(x - p.x) * 320 / p.w);
            const int sy = l.scale ? (y - p.y) / l.scale : int(int64_t(y - p.y) * 240 / p.h);
            frame[size_t(y) * size_t(l.output_w) + size_t(x)] = conv(panel[size_t(sy) * 320 + size_t(sx)]);
        }
    return frame;
}

void frame_checker() {
    const std::vector<uint16_t> panel = test_panel();
    struct Case {
        int w, h;
        Scaling s;
    };
    const Case cases[] = {{1920, 1080, Scaling::Framed}, {2560, 1440, Scaling::Framed}, {1008, 768, Scaling::Framed},
                          {1366, 768, Scaling::Integer}, {1920, 1080, Scaling::Fit},    {1367, 769, Scaling::Framed},
                          {200, 150, Scaling::Framed}};
    for (const Case &c : cases) {
        const PresentationLayout l = layout_presentation(c.w, c.h, c.s);
        const std::string name = describe_layout(l);
        const std::vector<uint32_t> a = draw(l, panel, expand), b = draw(l, panel, unorm);
        const std::string ra = uv::check_presented_frame(l, panel.data(), a.data(), c.w, c.h);
        const std::string rb = uv::check_presented_frame(l, panel.data(), b.data(), c.w, c.h);
        check(ra.empty() && rb.empty(), "C1 a frame drawn from the layout passes (bit-replicated and UNORM colours): " + name,
              ra + " | " + rb);
    }
    // Wrong frames are caught.
    const PresentationLayout l = layout_presentation(1920, 1080, Scaling::Framed);
    std::vector<uint32_t> f = draw(l, panel, expand);
    const std::vector<uint32_t> good = f;
    auto at = [&](int x, int y) -> uint32_t & { return f[size_t(y) * 1920 + size_t(x)]; };
    at(10, 10) = 0x000001 << 3; // letterbox not black
    check(!uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080).empty(), "C2 a non-black letterbox pixel is caught");
    f = good;
    at(292, 500) = expand(0x0016); // the band one level off
    check(!uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080).empty(), "C3 a band one blue level off is caught");
    f = good;
    at(292, 32) = expand(uv::kRingBandRgb565); // a cut corner filled in
    check(!uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080).empty(), "C4 a filled-in cut corner is caught");
    f = good;
    at(320 + 3, 60) = expand(uint16_t(panel[0] ^ 0x0800)); // one pixel of the first 4x4 block differs
    check(!uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080).empty(),
          "C5 a non-uniform 4x4 block in the picture is caught");
    // The picture one pixel off centre (the 1 px asymmetry SDL's float centring gives).
    PresentationLayout shifted = l;
    shifted.picture.x += 1;
    for (int i = 0; i < shifted.piece_count; ++i) shifted.pieces[i].rect.x += 1;
    const std::vector<uint32_t> s = draw(shifted, panel, expand);
    check(!uv::check_presented_frame(l, panel.data(), s.data(), 1920, 1080).empty(), "C6 a frame one pixel off centre is caught");
    // A whole-pixel picture checked against a framed layout (the ring missing).
    const std::vector<uint32_t> bare = draw(layout_presentation(1920, 1080, Scaling::Integer), panel, expand);
    check(!uv::check_presented_frame(l, panel.data(), bare.data(), 1920, 1080).empty(), "C7 a frame without its ring is caught");
    check(!uv::check_presented_frame(l, panel.data(), good.data(), 1366, 768).empty(), "C8 a frame of another size is refused");
    // The ring's band one 8-bit level off the picture's band (0,0,172 beside
    // 0,0,173: the same RGB565 value, as SDL_SetRenderDrawColor could give).
    f = good;
    for (int i = 0; i < l.piece_count; ++i) {
        if (l.pieces[i].colour != RingColour::Band) continue;
        const PixelRect &r = l.pieces[i].rect;
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) at(x, y) = 0x0000AC;
    }
    const std::string level = uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080);
    check(!level.empty() && expand(0x0015) == 0x0000AD, "C9 a ring band one 8-bit level off the picture's band is caught", level);
}

// A fractional Fit as Direct3D 11 samples SDL_SCALEMODE_PIXELART: each screen
// pixel is the average of the texels under its footprint, weighted by area,
// in 8-bit colour -- even texels with a one-pixel blend where they meet.
std::vector<uint32_t> draw_pixelart(const PresentationLayout &l, const std::vector<uint16_t> &panel) {
    std::vector<uint32_t> frame(size_t(l.output_w) * size_t(l.output_h), 0);
    const PixelRect &p = l.picture;
    // The texels under screen pixels [i, i+1) of an axis of n texels over `span` pixels, with their weights.
    auto footprint = [](int i, int n, int span, int &first, double w[2]) {
        const double a = double(i) * n / span, b = double(i + 1) * n / span;
        first = int(a);
        const double split = std::min(b, double(first + 1));
        w[0] = (split - a) / (b - a);
        w[1] = 1.0 - w[0];
    };
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            int tx = 0, ty = 0;
            double wx[2], wy[2];
            footprint(x, 320, p.w, tx, wx);
            footprint(y, 240, p.h, ty, wy);
            double sum[3] = {0, 0, 0};
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i) {
                    const uint32_t c = unorm(panel[size_t(std::min(ty + j, 239)) * 320 + size_t(std::min(tx + i, 319))]);
                    const double k = wx[i] * wy[j];
                    sum[0] += k * double(c >> 16);
                    sum[1] += k * double((c >> 8) & 0xFF);
                    sum[2] += k * double(c & 0xFF);
                }
            frame[size_t(p.y + y) * size_t(l.output_w) + size_t(p.x + x)] =
                uint32_t(std::lround(sum[0])) << 16 | uint32_t(std::lround(sum[1])) << 8 | uint32_t(std::lround(sum[2]));
        }
    return frame;
}

void pixelart_fit() {
    // A picture of 7x7 blocks in the chrome's colours: blends at the block
    // edges (7 texels never span whole screen pixels at these scales), plain colour inside.
    std::vector<uint16_t> panel(320 * 240);
    const uint16_t colours[] = {0x0000, 0xFFFF, 0x0015, 0x52AA, 0x07FF, 0xF800, 0xAD55};
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) panel[size_t(y) * 320 + size_t(x)] = colours[size_t((x / 7) * 3 + (y / 7) * 5) % 7];
    for (const int w : {1920, 1000, 1366}) {
        const int h = w == 1920 ? 1080 : w == 1000 ? 750 : 768;
        const PresentationLayout l = layout_presentation(w, h, Scaling::Fit);
        const std::vector<uint32_t> f = draw_pixelart(l, panel);
        bool blends = false; // the frame really has colours that are no texel's
        for (const uint32_t c : f)
            blends = blends || std::none_of(std::begin(colours), std::end(colours), [&](uint16_t t) { return unorm(t) == c; });
        const std::string r = uv::check_presented_frame(l, panel.data(), f.data(), w, h);
        check(r.empty() && blends && l.scale == 0,
              "C10 a fractional Fit sampled as pixel art (blended texel edges, as Direct3D 11 draws it) passes: " +
                  describe_layout(l),
              r);
    }
    // Inside a block every texel around is the same colour: anything else is caught.
    const PresentationLayout l = layout_presentation(1920, 1080, Scaling::Fit);
    std::vector<uint32_t> f = draw_pixelart(l, panel);
    const int x = l.picture.x + int(4.5 * 4.5), y = l.picture.y + int(4.5 * 4.5); // texel (4, 4): block (0, 0), colour 0x0000
    f[size_t(y) * 1920 + size_t(x)] = 0x404040;
    check(!uv::check_presented_frame(l, panel.data(), f.data(), 1920, 1080).empty(),
          "C11 a fractional Fit pixel no blend of the texels around it can give is caught");
}

// --- 4. The Settings page's Scaling row -------------------------------------------------
struct ProfileProbe {
    uv::DesktopSettings settings;
    int changes = 0;
    uv::DesktopProfile::Change last = uv::DesktopProfile::Change::Interface;
    static void changed(void *context, uv::DesktopProfile::Change what) {
        auto *self = static_cast<ProfileProbe *>(context);
        ++self->changes;
        self->last = what;
    }
};

std::string row_label(size_t row, const char **footer = nullptr) {
    const openu5::PlatformRow &r = openu5::platform_profile().platform_rows[row];
    char text[64] = {};
    const char *f = r.label ? r.label(r.context, text, sizeof(text)) : nullptr;
    if (footer) *footer = f;
    return text;
}

bool adjust(size_t row, int delta) {
    const openu5::PlatformRow &r = openu5::platform_profile().platform_rows[row];
    return r.adjust && r.adjust(r.context, delta);
}

void scaling_row() {
    ProfileProbe probe;
    uv::DesktopProfile profile;
    uv::DesktopProfile::Hooks hooks;
    hooks.settings = &probe.settings;
    hooks.context = &probe;
    hooks.changed = &ProfileProbe::changed;
    profile.install(hooks);
    const char *footer = nullptr;
    check(row_label(2, &footer) == "Scaling: Framed" && footer && std::string(footer) == "Whole pixels, centred in a thin blue frame",
          "P1 the default Scaling row reads \"Scaling: Framed\" with its footer", row_label(2));
    // Right / Enter: Framed -> Whole pixels -> Fit window -> Framed.
    const char *want_right[] = {"Scaling: Whole pixels", "Scaling: Fit window", "Scaling: Framed"};
    const Scaling want_value[] = {Scaling::Integer, Scaling::Fit, Scaling::Framed};
    for (int i = 0; i < 3; ++i) {
        const bool changed = adjust(2, +1);
        check(changed && row_label(2) == want_right[i] && probe.settings.scaling == want_value[i] &&
                  probe.last == uv::DesktopProfile::Change::Scaling && probe.changes == i + 1,
              format("P2 right #%d: %s", i + 1, want_right[i]), row_label(2));
    }
    // Left goes back: Framed -> Fit window -> Whole pixels -> Framed.
    const char *want_left[] = {"Scaling: Fit window", "Scaling: Whole pixels", "Scaling: Framed"};
    for (int i = 0; i < 3; ++i) {
        adjust(2, -1);
        check(row_label(2) == want_left[i], format("P3 left #%d: %s", i + 1, want_left[i]), row_label(2));
    }
    probe.settings.scaling = Scaling::Integer;
    check(row_label(2, &footer) == "Scaling: Whole pixels" && std::string(footer) == "Every pixel the same size; may letterbox",
          "P4 Whole pixels keeps its footer");
    probe.settings.scaling = Scaling::Fit;
    check(row_label(2, &footer) == "Scaling: Fit window" && std::string(footer) == "Fills the window; pixels may differ in size",
          "P5 Fit window keeps its footer");
    openu5::set_platform_profile(openu5::PlatformProfile{});
}

// --- 5. The window: "Window N x" ------------------------------------------------------
std::string plan_text(const uv::WindowPlan &p) {
    return format("%dx %s %dx%d at %d,%d aspect %d:%d", p.scale, p.framed ? "framed" : "bare", p.client_w, p.client_h, p.x,
                  p.y, p.aspect_w, p.aspect_h);
}

void window_plans() {
    using uv::WindowBorders;
    using uv::WorkArea;
    // Work areas with a 48/72/96 px taskbar; frames as Windows 11 gives them at
    // 100 / 150 / 200 % (caption + sizing frame on top, sizing frame elsewhere).
    const WorkArea hd{0, 0, 1920, 1032}, hd150{0, 0, 1920, 1008}, small{0, 0, 1366, 728}, qhd{0, 0, 2560, 1392},
        uhd200{0, 0, 3840, 2064}, second{1920, -200, 1280, 984}, tiny{0, 0, 300, 200}, offscreen{0, 0, 1024, 768};
    const WindowBorders b100{31, 8, 8, 8}, b150{46, 11, 11, 11}, b200{62, 16, 16, 16}, none{};
    struct Row {
        const char *what;
        WorkArea area;
        WindowBorders borders;
        int request;
        Scaling scaling;
        const char *want;
    };
    const Row rows[] = {
        {"1080p 100%: Window 3x is framed", hd, b100, 3, Scaling::Framed, "3x framed 1008x768 at 456,143 aspect 336:256"},
        {"1080p 100%: Window 4x does not fit framed: bare", hd, b100, 4, Scaling::Framed, "4x bare 1280x960 at 320,47 aspect 320:240"},
        {"1080p 100%: Window 8x gets 4x (the effective scale)", hd, b100, 8, Scaling::Framed, "4x bare 1280x960 at 320,47 aspect 320:240"},
        {"1080p 100%: Whole pixels 3x is bare", hd, b100, 3, Scaling::Integer, "3x bare 960x720 at 480,167 aspect 320:240"},
        {"1080p 100%: Fit 3x is bare", hd, b100, 3, Scaling::Fit, "3x bare 960x720 at 480,167 aspect 320:240"},
        {"1080p 150%: 4x does not fit with a 46 px caption: 3x framed", hd150, b150, 4, Scaling::Framed,
         "3x framed 1008x768 at 456,137 aspect 336:256"},
        {"1366x768 100%: Window 3x gets 2x framed (it once showed 'Window 3x')", small, b100, 3, Scaling::Framed,
         "2x framed 672x512 at 347,119 aspect 336:256"},
        {"1440p 100%: Window 6x gets 5x framed", qhd, b100, 6, Scaling::Framed, "5x framed 1680x1280 at 440,67 aspect 336:256"},
        {"4K 200%: Window 6x framed", uhd200, b200, 6, Scaling::Framed, "6x framed 2016x1536 at 912,287 aspect 336:256"},
        {"a second display left of / above the first", second, b100, 3, Scaling::Framed, "3x framed 1008x768 at 2056,-81 aspect 336:256"},
        {"nothing fits: 1x bare against the top-left, title bar inside", tiny, b100, 3, Scaling::Framed,
         "1x bare 320x240 at 8,31 aspect 320:240"},
        {"headless (offscreen 1024x768, no frame): 3x framed fills it", offscreen, none, 3, Scaling::Framed,
         "3x framed 1008x768 at 8,0 aspect 336:256"},
        {"headless: 4x gets 3x", offscreen, none, 4, Scaling::Framed, "3x framed 1008x768 at 8,0 aspect 336:256"},
        {"a request of 0 is 1x", hd, b100, 0, Scaling::Framed, "1x framed 336x256 at 792,399 aspect 336:256"},
    };
    for (const Row &r : rows) {
        const std::string got = plan_text(uv::plan_window(r.request, r.scaling, r.area, r.borders));
        check(got == r.want, std::string("W1 ") + r.what + ": " + r.want, "got " + got);
    }
    check(uv::largest_window_scale(hd, b100) == 4 && uv::largest_window_scale(hd150, b150) == 3 &&
              uv::largest_window_scale(small, b100) == 2 && uv::largest_window_scale(qhd, b100) == 5 &&
              uv::largest_window_scale(uhd200, b200) == 8 && uv::largest_window_scale(tiny, b100) == 1 &&
              uv::largest_window_scale(offscreen, none) == 3,
          "W2 largest window scale: 1080p 4, 1080p@150% 3, 1366x768 2, 1440p 5, 4K@200% 8, tiny 1, headless 3");
    // The first-run default: floor(3 x content scale), within what fits.
    const WorkArea huge{0, 0, 1 << 15, 1 << 15};
    struct Dpi {
        float scale;
        int want;
    };
    const Dpi dpis[] = {{1.0f, 3}, {1.25f, 3}, {1.5f, 4}, {1.75f, 5}, {2.0f, 6}, {2.25f, 6}, {2.5f, 7}, {3.0f, 8}, {4.0f, 8}};
    for (const Dpi &d : dpis)
        check(uv::default_window_scale(d.scale, huge, none) == d.want,
              format("W3 default window scale at %.0f %% display scale: %dx", double(d.scale) * 100, d.want),
              format("got %d", uv::default_window_scale(d.scale, huge, none)));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    check(uv::default_window_scale(0.0f, huge, none) == 3 && uv::default_window_scale(nan, huge, none) == 3 &&
              uv::default_window_scale(-2.0f, huge, none) == 3,
          "W4 an unreadable display scale (0, NaN, negative) counts as 100 %: 3x");
    check(uv::default_window_scale(1.5f, hd150, b150) == 3 && uv::default_window_scale(2.0f, hd, b100) == 4 &&
              uv::default_window_scale(1.0f, small, b100) == 2,
          "W5 the default never exceeds what fits (1080p@150%: 3x, 1080p at 200%: 4x, 1366x768: 2x)");

    // Every combination against the rules.
    Failure f;
    int n = 0;
    const WindowBorders frames[] = {none, b100, b150, b200, {1, 2, 3, 4}};
    for (int w = 300; w <= 4000; w += 37)
        for (int h = 200; h <= 2400; h += 29)
            for (const WindowBorders &b : frames)
                for (int request = 0; request <= 9; ++request)
                    for (Scaling s : {Scaling::Framed, Scaling::Integer, Scaling::Fit}) {
                        ++n;
                        const WorkArea a{w / 3 - 500, -h / 5, w, h};
                        const uv::WindowPlan p = uv::plan_window(request, s, a, b);
                        const std::string at = format("area %dx%d at %d,%d frame %d/%d/%d/%d, %dx %s: ", w, h, a.x, a.y, b.top,
                                                      b.left, b.bottom, b.right, request, uv::scaling_name(s)) + plan_text(p);
                        const int clamped = std::clamp(request, 1, 8);
                        auto fits = [&](int cw, int ch) { return cw + b.left + b.right <= w && ch + b.top + b.bottom <= h; };
                        if (p.scale < 1 || p.scale > clamped) f.note(at + " (scale out of range)");
                        for (int k = p.scale + 1; k <= clamped; ++k)
                            if (fits(320 * k, 240 * k)) f.note(at + format(" (%dx would fit)", k));
                        if (p.framed && s != Scaling::Framed) f.note(at + " (framed outside Framed)");
                        if (s == Scaling::Framed && !p.framed && fits(336 * p.scale, 256 * p.scale)) f.note(at + " (framed would fit)");
                        const int margin = p.framed ? 8 : 0;
                        if (p.client_w != (320 + 2 * margin) * p.scale || p.client_h != (240 + 2 * margin) * p.scale ||
                            p.aspect_w != 320 + 2 * margin || p.aspect_h != 240 + 2 * margin)
                            f.note(at + " (client is not the aspect times the scale)");
                        // Per axis: the frame centred when it fits that axis, else against the area's start.
                        if (!fits(p.client_w, p.client_h) && p.scale != 1) f.note(at + " (an oversized window above 1x)");
                        const bool fits_w = p.client_w + b.left + b.right <= w, fits_h = p.client_h + b.top + b.bottom <= h;
                        const int l = p.x - b.left - a.x, r = a.x + w - (p.x + p.client_w + b.right);
                        const int t = p.y - b.top - a.y, bo = a.y + h - (p.y + p.client_h + b.bottom);
                        if (fits_w ? (l < 0 || r - l < 0 || r - l > 1) : p.x != a.x + b.left)
                            f.note(at + " (horizontal placement)");
                        if (fits_h ? (t < 0 || bo - t < 0 || bo - t > 1) : p.y != a.y + b.top)
                            f.note(at + " (vertical placement: the title bar must stay inside)");
                        if (p.scale != uv::plan_window(request, Scaling::Integer, a, b).scale)
                            f.note(at + " (the effective scale depends on the scaling)");
                        if (uv::client_window_scale(p.client_w, p.client_h) != p.scale)
                            f.note(at + " (its client does not show its own N back)");
                    }
    check(f.count == 0, format("W6 %d window plans (work areas, frames, requests 0..9, three scalings) obey the rules", n),
          format("%d failures, first: %s", f.count, f.first.c_str()));

    // The N a client shows (the Display row after a drag or a snap).
    struct Client {
        int w, h, want;
    };
    const Client clients[] = {{1008, 768, 3}, {960, 720, 3}, {336, 256, 1}, {672, 512, 2}, {2688, 2048, 8},
                              {700, 533, 2},  {1920, 1080, 4}, {2560, 1440, 6}, {320, 240, 1}, {319, 239, 1},
                              {0, 0, 1},      {5000, 5000, 8}, {1279, 960, 3}};
    for (const Client &c : clients)
        check(uv::client_window_scale(c.w, c.h) == c.want, format("W7 a %dx%d client shows Window %dx", c.w, c.h, c.want),
              format("got %d", uv::client_window_scale(c.w, c.h)));

    // A window that changes shape keeps its centre, inside the work area.
    struct Move {
        const char *what;
        WorkArea area;
        WindowBorders borders;
        int x, y, w, h;
        int request;
        Scaling scaling;
        const char *want;
    };
    const Move moves[] = {
        {"a centred 3x bare window framed: the centred framed plan", hd, b100, 480, 167, 960, 720, 3, Scaling::Framed,
         "3x framed 1008x768 at 456,143 aspect 336:256"},
        {"... and bare again: back where it was", hd, b100, 456, 143, 1008, 768, 3, Scaling::Integer,
         "3x bare 960x720 at 480,167 aspect 320:240"},
        {"a window moved up-left: its title bar stays inside", hd, b100, 100, 50, 960, 720, 3, Scaling::Framed,
         "3x framed 1008x768 at 76,31 aspect 336:256"},
        {"a window against the bottom-right: its frame stays inside", hd, b100, 950, 300, 960, 720, 3, Scaling::Framed,
         "3x framed 1008x768 at 904,256 aspect 336:256"},
        {"a dragged window (any size) is centred on its own centre", hd, b100, 300, 100, 1400, 900, 2, Scaling::Framed,
         "2x framed 672x512 at 664,294 aspect 336:256"},
        {"a second display left of / above the first", second, b100, 2000, -100, 640, 480, 2, Scaling::Framed,
         "2x framed 672x512 at 1984,-116 aspect 336:256"},
        {"a big dragged window on a small display: the plan at its centre, pushed up inside", small, b100, 0, 0, 1280, 960,
         4, Scaling::Framed, "2x framed 672x512 at 304,208 aspect 336:256"},
        {"a window larger than the area: against its top-left, title bar inside", tiny, b100, 50, 50, 320, 240, 3,
         Scaling::Framed, "1x bare 320x240 at 8,31 aspect 320:240"},
        {"headless (no frame)", offscreen, none, 0, 0, 960, 720, 3, Scaling::Framed, "3x framed 1008x768 at 0,0 aspect 336:256"},
    };
    for (const Move &m : moves) {
        const uv::WindowPlan p = uv::keep_centre(uv::plan_window(m.request, m.scaling, m.area, m.borders), m.x, m.y, m.w, m.h,
                                                 m.area, m.borders);
        check(plan_text(p) == m.want, std::string("W8 ") + m.what + ": " + m.want, "got " + plan_text(p));
    }
    // Over the W6 grid: the frame inside the area whenever it fits, and the centre kept when it can be.
    Failure k;
    n = 0;
    for (int w = 400; w <= 4000; w += 211)
        for (int h = 300; h <= 2400; h += 157)
            for (const WindowBorders &b : frames)
                for (int x = -300; x <= 2000; x += 233)
                    for (int y = -300; y <= 1500; y += 197)
                        for (Scaling s : {Scaling::Framed, Scaling::Integer}) {
                            ++n;
                            const WorkArea a{-100, 50, w, h};
                            const uv::WindowPlan plan = uv::plan_window(3, s, a, b);
                            const uv::WindowPlan p = uv::keep_centre(plan, x, y, 700, 500, a, b);
                            const std::string at = format("area %dx%d frame %d/%d/%d/%d window 700x500 at %d,%d: ", w, h,
                                                          b.top, b.left, b.bottom, b.right, x, y) + plan_text(p);
                            if (p.client_w != plan.client_w || p.client_h != plan.client_h || p.scale != plan.scale)
                                k.note(at + " (the plan's size changed)");
                            const bool fits_w = p.client_w + b.left + b.right <= w, fits_h = p.client_h + b.top + b.bottom <= h;
                            if (fits_w ? (p.x - b.left < a.x || p.x + p.client_w + b.right > a.x + w) : p.x != a.x + b.left)
                                k.note(at + " (horizontal: frame outside the area)");
                            if (fits_h ? (p.y - b.top < a.y || p.y + p.client_h + b.bottom > a.y + h) : p.y != a.y + b.top)
                                k.note(at + " (vertical: frame outside the area)");
                            // The old centre, moved only as far as the area needs.
                            const int cx = x + 350 - p.client_w / 2, cy = y + 250 - p.client_h / 2;
                            if (fits_w && p.x != std::clamp(cx, a.x + b.left, a.x + w - b.right - p.client_w))
                                k.note(at + " (horizontal: not the old centre, moved only as needed)");
                            if (fits_h && p.y != std::clamp(cy, a.y + b.top, a.y + h - b.bottom - p.client_h))
                                k.note(at + " (vertical: not the old centre, moved only as needed)");
                        }
    check(k.count == 0, format("W9 %d shape changes keep the centre and the frame inside the work area", n),
          format("%d failures, first: %s", k.count, k.first.c_str()));
}

// --- 6. The Settings page's Display row --------------------------------------------------
uint8_t g_largest = 4;
uint8_t largest_hook(void *) { return g_largest; }

void display_row() {
    ProfileProbe probe;
    probe.settings.window_scale = 3;
    uv::DesktopProfile profile;
    uv::DesktopProfile::Hooks hooks;
    hooks.settings = &probe.settings;
    hooks.context = &probe;
    hooks.changed = &ProfileProbe::changed;
    hooks.largest_window_scale = &largest_hook;
    profile.install(hooks);
    const char *footer = nullptr;
    check(row_label(1, &footer) == "Display: Window 3x" && footer &&
              std::string(footer) == "Left/right: window size or fullscreen (Alt+Enter)",
          "D1 the Display row shows the window's scale", row_label(1));
    // The largest that fits is 4: right steps 4x, Fullscreen, 1x, 2x (no jump from 1x to 3x).
    const char *right[] = {"Display: Window 4x", "Display: Fullscreen", "Display: Window 1x", "Display: Window 2x", "Display: Window 3x"};
    for (int i = 0; i < 5; ++i) {
        adjust(1, +1);
        check(row_label(1) == right[i] && probe.last == uv::DesktopProfile::Change::Display && probe.changes == i + 1,
              format("D2 right #%d (largest 4): %s", i + 1, right[i]), row_label(1));
    }
    const char *left[] = {"Display: Window 2x", "Display: Window 1x", "Display: Fullscreen", "Display: Window 4x"};
    for (int i = 0; i < 4; ++i) {
        adjust(1, -1);
        check(row_label(1) == left[i], format("D3 left #%d: %s", i + 1, left[i]), row_label(1));
    }
    check(probe.settings.window_scale == 4 && !probe.settings.fullscreen, "D4 the row writes the scale and leaves fullscreen");
    // A display that holds less: the same row stops there.
    g_largest = 2;
    probe.settings.window_scale = 2;
    adjust(1, +1);
    check(probe.settings.fullscreen, "D5 at the largest that fits (2x), right goes to Fullscreen", row_label(1));
    adjust(1, -1);
    check(!probe.settings.fullscreen && probe.settings.window_scale == 2, "D6 left from Fullscreen goes to the largest (2x)",
          row_label(1));
    probe.settings.window_scale = 5; // a scale saved on a larger display
    adjust(1, -1);
    check(probe.settings.window_scale == 1, "D7 a scale above the largest steps as the largest (5x on a 2x display: left -> 1x)",
          row_label(1));
    // Without the hook the row spans 1..8.
    hooks.largest_window_scale = nullptr;
    profile.install(hooks);
    probe.settings.fullscreen = false;
    probe.settings.window_scale = 8;
    adjust(1, +1);
    check(probe.settings.fullscreen, "D8 without a display hook the row spans 1x..8x, then Fullscreen");
    openu5::set_platform_profile(openu5::PlatformProfile{});
}

} // namespace

int main() {
    std::printf("uv_presentation_test: the picture's placement and the Classic Framed ring\n");
    exact_sizes();
    every_size();
    frame_checker();
    pixelart_fit();
    scaling_row();
    window_plans();
    display_row();
    return uvtest::finish("uv_presentation_test");
}
