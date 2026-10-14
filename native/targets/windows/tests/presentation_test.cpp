// Ultima V Native for Windows -- the presentation (app/presentation_layout.h).
//
// The picture's placement and the Classic Framed ring, as pure arithmetic:
// exact layouts for the common desktop and window sizes, then every output
// size on a dense grid (and every size up to 1100x850) against the rules --
// whole-number scale, centring with the odd pixel on the right/bottom, the
// ring's three forms and their thresholds, nothing outside the output,
// nothing over the picture, no overlaps, mirror symmetry -- and finally the
// frame checker the --autotest window_layout step uses, against frames drawn
// here from the layout (and against deliberately wrong ones).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/presentation_layout.h"
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
}

} // namespace

int main() {
    std::printf("uv_presentation_test: the picture's placement and the Classic Framed ring\n");
    exact_sizes();
    every_size();
    frame_checker();
    return uvtest::finish("uv_presentation_test");
}
