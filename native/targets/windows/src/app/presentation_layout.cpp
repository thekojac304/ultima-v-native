#include "app/presentation_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace uv {
namespace {

void add(PresentationLayout &l, int x, int y, int w, int h, RingColour colour) {
    if (l.piece_count < kMaxRingPieces) l.pieces[l.piece_count++] = RingPiece{PixelRect{x, y, w, h}, colour};
}

// The ring around a picture at (x0, y0), scale u: rules, then the band
// rows (cut at both ends) and the band's sides.
void full_ring(PresentationLayout &l, int x0, int y0, int u) {
    const int in = (kRingGap + kRingRule) * u; // the rule's outer edge, from the picture
    const int pw = kPictureW * u, ph = kPictureH * u;
    add(l, x0 - in, y0 - in, pw + 2 * in, kRingRule * u, RingColour::Rule);           // rule top
    add(l, x0 - in, y0 + ph + kRingGap * u, pw + 2 * in, kRingRule * u, RingColour::Rule); // rule bottom
    add(l, x0 - in, y0 - kRingGap * u, kRingRule * u, ph + 2 * kRingGap * u, RingColour::Rule);          // rule left
    add(l, x0 + pw + kRingGap * u, y0 - kRingGap * u, kRingRule * u, ph + 2 * kRingGap * u, RingColour::Rule); // rule right
    const int reach = kRingReach * u;
    for (int row = 0; row < kRingBand; ++row) { // row 0 is the outermost
        const int cut = kRingCornerCut[row] * u, width = pw + 2 * reach - 2 * cut;
        add(l, x0 - reach + cut, y0 - reach + row * u, width, u, RingColour::Band);          // band top
        add(l, x0 - reach + cut, y0 + ph + reach - (row + 1) * u, width, u, RingColour::Band); // band bottom
    }
    add(l, x0 - reach, y0 - in, kRingBand * u, ph + 2 * in, RingColour::Band);       // band left
    add(l, x0 + pw + in, y0 - in, kRingBand * u, ph + 2 * in, RingColour::Band);     // band right
}

// The ring's sides only, the output's full height.
void pillars(PresentationLayout &l, int x0, int u, int output_h) {
    const int in = (kRingGap + kRingRule) * u, pw = kPictureW * u, reach = kRingReach * u;
    add(l, x0 - reach, 0, kRingBand * u, output_h, RingColour::Band);
    add(l, x0 - in, 0, kRingRule * u, output_h, RingColour::Rule);
    add(l, x0 + pw + kRingGap * u, 0, kRingRule * u, output_h, RingColour::Rule);
    add(l, x0 + pw + in, 0, kRingBand * u, output_h, RingColour::Band);
}

uint16_t to_rgb565(uint32_t rgb) {
    return uint16_t(((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F));
}

bool window_fits(int client_w, int client_h, const WorkArea &a, const WindowBorders &b) {
    return client_w + b.left + b.right <= a.w && client_h + b.top + b.bottom <= a.h;
}

// One axis of the client's position: the frame centred in the span, or
// against its start when the frame is larger.
int place(int origin, int span, int client, int before, int after) {
    const int outer = client + before + after;
    return origin + before + (outer <= span ? (span - outer) / 2 : 0);
}

} // namespace

PresentationLayout layout_presentation(int output_w, int output_h, Scaling scaling) {
    PresentationLayout l;
    l.output_w = std::max(0, output_w);
    l.output_h = std::max(0, output_h);
    l.scaling = scaling;
    const int W = l.output_w, H = l.output_h;
    if (scaling == Scaling::Fit) {
        // The largest 4:3 rectangle, in whole pixels, centred.
        int w = W, h = H;
        if (int64_t(W) * kPictureH >= int64_t(H) * kPictureW) w = int(int64_t(H) * kPictureW / kPictureH);
        else h = int(int64_t(W) * kPictureH / kPictureW);
        l.picture = PixelRect{(W - w) / 2, (H - h) / 2, w, h};
        l.scale = (w % kPictureW == 0 && h % kPictureH == 0 && w / kPictureW == h / kPictureH) ? w / kPictureW : 0;
        return l;
    }
    // Whole pixels; never smaller than 1, even in an output smaller than the picture.
    const int u = std::max(1, std::min(W / kPictureW, H / kPictureH));
    const int pw = kPictureW * u, ph = kPictureH * u;
    // Floor division (also for a negative leftover): the odd pixel goes right / down.
    auto half = [](int leftover) { return leftover >= 0 ? leftover / 2 : -((1 - leftover) / 2); };
    const int x0 = half(W - pw), y0 = half(H - ph);
    l.scale = u;
    l.picture = PixelRect{x0, y0, pw, ph};
    if (scaling != Scaling::Framed) return l;
    // Both sides of an axis or neither: the left / top margin is the smaller
    // one. No ring beside a picture the output cannot hold (smaller than 320x240).
    const int reach = kRingReach * u;
    if (x0 < reach || y0 < 0) return l;
    if (y0 >= reach) {
        l.ring = RingForm::Full;
        full_ring(l, x0, y0, u);
    } else {
        l.ring = RingForm::Pillars;
        pillars(l, x0, u, H);
    }
    return l;
}

const char *ring_form_name(RingForm form) {
    switch (form) {
    case RingForm::Full: return "full";
    case RingForm::Pillars: return "pillars";
    default: return "none";
    }
}

std::string describe_layout(const PresentationLayout &l) {
    char text[96];
    std::snprintf(text, sizeof(text), "%dx%d %s %dx%d@%d,%d", l.output_w, l.output_h,
                  l.scaling == Scaling::Fit ? "fit" : ring_form_name(l.ring), l.picture.w, l.picture.h, l.picture.x,
                  l.picture.y);
    return text;
}

std::string check_presented_frame(const PresentationLayout &l, const uint16_t *panel, const uint32_t *frame, int w,
                                  int h) {
    char text[200];
    if (w != l.output_w || h != l.output_h) {
        std::snprintf(text, sizeof(text), "frame is %dx%d, the layout's output %dx%d", w, h, l.output_w, l.output_h);
        return text;
    }
    const PixelRect &p = l.picture;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const uint16_t got = to_rgb565(frame[size_t(y) * size_t(w) + size_t(x)]);
            const char *where = "letterbox";
            bool ok = false;
            uint16_t want = 0;
            if (p.contains(x, y)) {
                where = "picture";
                if (l.scale > 0) {
                    want = panel[size_t((y - p.y) / l.scale) * kPictureW + size_t((x - p.x) / l.scale)];
                    ok = got == want;
                } else {
                    // Fractional Fit: the nearest texel or one of its neighbours.
                    const int sx = int(int64_t(x - p.x) * kPictureW / p.w), sy = int(int64_t(y - p.y) * kPictureH / p.h);
                    want = panel[size_t(sy) * kPictureW + size_t(sx)];
                    for (int dy = -1; dy <= 1 && !ok; ++dy)
                        for (int dx = -1; dx <= 1 && !ok; ++dx) {
                            const int tx = std::clamp(sx + dx, 0, kPictureW - 1), ty = std::clamp(sy + dy, 0, kPictureH - 1);
                            ok = got == panel[size_t(ty) * kPictureW + size_t(tx)];
                        }
                }
            } else {
                for (int i = 0; i < l.piece_count; ++i)
                    if (l.pieces[i].rect.contains(x, y)) {
                        where = l.pieces[i].colour == RingColour::Rule ? "ring rule" : "ring band";
                        want = l.pieces[i].colour == RingColour::Rule ? kRingRuleRgb565 : kRingBandRgb565;
                        break;
                    }
                ok = got == want;
            }
            if (!ok) {
                std::snprintf(text, sizeof(text), "%s pixel %d,%d is %04x (0x%06x), expected %04x", where, x, y,
                              unsigned(got), unsigned(frame[size_t(y) * size_t(w) + size_t(x)]), unsigned(want));
                return text;
            }
        }
    }
    return std::string();
}

WindowPlan plan_window(int requested_scale, Scaling scaling, const WorkArea &area, const WindowBorders &borders) {
    WindowPlan p;
    // Framed N, bare N, framed N-1, ...; at 1 the bare window even when it does not fit.
    for (p.scale = std::clamp(requested_scale, int(kMinWindowScale), int(kMaxWindowScale));; --p.scale) {
        p.framed = scaling == Scaling::Framed &&
                   window_fits((kPictureW + 2 * kFramedMargin) * p.scale, (kPictureH + 2 * kFramedMargin) * p.scale, area, borders);
        if (p.framed || p.scale == 1 || window_fits(kPictureW * p.scale, kPictureH * p.scale, area, borders)) break;
    }
    const int margin = p.framed ? kFramedMargin : 0;
    p.aspect_w = kPictureW + 2 * margin;
    p.aspect_h = kPictureH + 2 * margin;
    p.client_w = p.aspect_w * p.scale;
    p.client_h = p.aspect_h * p.scale;
    p.x = place(area.x, area.w, p.client_w, borders.left, borders.right);
    p.y = place(area.y, area.h, p.client_h, borders.top, borders.bottom);
    return p;
}

int largest_window_scale(const WorkArea &area, const WindowBorders &borders) {
    int n = kMaxWindowScale;
    while (n > 1 && !window_fits(kPictureW * n, kPictureH * n, area, borders)) --n;
    return n;
}

int default_window_scale(float content_scale, const WorkArea &area, const WindowBorders &borders) {
    // A display scale SDL could not read (0, NaN) or a nonsense one counts as 100 %.
    if (!(content_scale >= 0.5f && content_scale <= 8.0f)) content_scale = 1.0f;
    const int n = std::clamp(int(std::floor(3.0 * double(content_scale) + 1e-6)), int(kMinWindowScale), int(kMaxWindowScale));
    return std::min(n, largest_window_scale(area, borders));
}

} // namespace uv
