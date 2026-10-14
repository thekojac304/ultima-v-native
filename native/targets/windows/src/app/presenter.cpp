#include "app/presenter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>

#include "esp_log.h"
#include "platform/virtual_panel.h"

namespace uv {
namespace {
constexpr char kTag[] = "Presenter";
constexpr int kW = panel::kWidth, kH = panel::kHeight;
static_assert(kW == kPictureW && kH == kPictureH, "the layout places the virtual panel");

SDL_FRect frect(const PixelRect &r) { return SDL_FRect{float(r.x), float(r.y), float(r.w), float(r.h)}; }

// One of the ring's colours as a 1x1 RGB565 texture, drawn through the same
// path as the picture, so the band is exactly the in-game band's blue on any
// renderer. One texture per colour, stretched whole: one texel of a wider
// atlas, stretched over a band row a thousand pixels long, reaches the
// neighbouring texel at its far end within a GPU's sub-texel precision
// (Direct3D 11 drew the last 2-4 px of every band row white). A fractional
// source rectangle inside the texel is no way out: the software renderer
// truncates it to whole texels.
SDL_Texture *colour_texture(SDL_Renderer *renderer, uint16_t rgb565) {
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (t && (!SDL_UpdateTexture(t, nullptr, &rgb565, int(sizeof(rgb565))) || !SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST))) {
        SDL_DestroyTexture(t);
        t = nullptr;
    }
    return t;
}

/** A 24-bit BMP (bottom-up rows, BGR, rows padded to 4 bytes) of w x h 0xRRGGBB pixels. */
bool write_bmp(const std::wstring &path, int w, int h, const uint32_t *rgb) {
    const int row_bytes = (w * 3 + 3) & ~3;
    const uint32_t image = uint32_t(row_bytes) * uint32_t(h);
    uint8_t header[54]{};
    auto put32 = [&](int at, uint32_t v) {
        for (int i = 0; i < 4; ++i) header[at + i] = uint8_t(v >> (8 * i));
    };
    header[0] = 'B';
    header[1] = 'M';
    put32(2, 54 + image);
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(w));
    put32(22, uint32_t(h));
    header[26] = 1;
    header[28] = 24;
    put32(34, image);
    std::vector<uint8_t> pixels(image);
    for (int y = 0; y < h; ++y) {
        uint8_t *row = pixels.data() + size_t(h - 1 - y) * size_t(row_bytes);
        for (int x = 0; x < w; ++x) {
            const uint32_t p = rgb[size_t(y) * size_t(w) + size_t(x)];
            row[x * 3] = uint8_t(p);
            row[x * 3 + 1] = uint8_t(p >> 8);
            row[x * 3 + 2] = uint8_t(p >> 16);
        }
    }
    std::FILE *f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(header, 1, sizeof(header), f) == sizeof(header) &&
                    std::fwrite(pixels.data(), 1, pixels.size(), f) == pixels.size();
    return std::fclose(f) == 0 && ok;
}
} // namespace

Presenter::~Presenter() { destroy(); }

bool Presenter::create(const char *title, const DesktopSettings &settings) {
    destroy();
    // Created hidden at a provisional size: the real one needs the window's
    // display and frame (set_window_scale below), and nothing is seen before show().
    window_ = SDL_CreateWindow(title, kW * 3, kH * 3, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!window_) {
        error_ = std::string("SDL_CreateWindow: ") + SDL_GetError();
        return false;
    }
    SDL_SetWindowMinimumSize(window_, kW, kH);
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        error_ = std::string("SDL_CreateRenderer: ") + SDL_GetError();
        destroy();
        return false;
    }
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    ring_band_ = colour_texture(renderer_, kRingBandRgb565);
    ring_rule_ = colour_texture(renderer_, kRingRuleRgb565);
    if (!texture_ || !ring_band_ || !ring_rule_) {
        error_ = std::string("SDL_CreateTexture: ") + SDL_GetError();
        destroy();
        return false;
    }
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    texture_scale_mode_ = int(SDL_SCALEMODE_NEAREST);
    // Every rectangle is placed in whole output pixels here (layout_presentation),
    // never by SDL's logical presentation and its float centring.
    SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    ESP_LOGI(kTag, "renderer %s, display content scale %.2f", SDL_GetRendererName(renderer_), double(content_scale()));
    scaling_ = settings.scaling;
    set_vsync(settings.vsync);
    // No saved scale: the first-run default from the display's scale.
    set_window_scale(settings.window_scale == kAutoWindowScale ? default_window_scale() : settings.window_scale);
    if (settings.fullscreen) SDL_SetWindowFullscreen(window_, true);
    stale_ = true;
    return true;
}

void Presenter::destroy() {
    if (ring_rule_) SDL_DestroyTexture(ring_rule_);
    if (ring_band_) SDL_DestroyTexture(ring_band_);
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    ring_rule_ = nullptr;
    ring_band_ = nullptr;
    texture_ = nullptr;
    renderer_ = nullptr;
    window_ = nullptr;
    uploaded_generation_ = ~0ull;
}

void Presenter::show() {
    if (window_) SDL_ShowWindow(window_);
    stale_ = true;
}

void Presenter::upload() {
    // The T-Deck's Brightness (the panel's backlight duty) is not applied: a
    // monitor has its own, the desktop Settings page has no Brightness row,
    // and a card copied from a dimmed T-Deck must not darken the window. The
    // picture is drawn at full intensity, exactly as F12 saves it.
    const uint64_t generation = panel::generation();
    if (generation == uploaded_generation_) return;
    SDL_UpdateTexture(texture_, nullptr, panel::pixels(), kW * int(sizeof(uint16_t)));
    uploaded_generation_ = generation;
}

void Presenter::draw() {
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &w, &h);
    layout_ = layout_presentation(w, h, scaling_);
    // Whole-number scales sample nearest; a fractional Fit samples as pixel
    // art (even columns and rows), where the renderer has it -- SDL falls
    // back to nearest on renderers without it, and so does this.
    const SDL_ScaleMode mode = layout_.scale > 0 ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_PIXELART;
    if (int(mode) != texture_scale_mode_) {
        if (!SDL_SetTextureScaleMode(texture_, mode)) SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        texture_scale_mode_ = int(mode);
    }
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    for (int i = 0; i < layout_.piece_count; ++i) {
        const RingPiece &piece = layout_.pieces[i];
        const SDL_FRect dst = frect(piece.rect);
        SDL_RenderTexture(renderer_, piece.colour == RingColour::Band ? ring_band_ : ring_rule_, nullptr, &dst);
    }
    const SDL_FRect picture = frect(layout_.picture);
    SDL_RenderTexture(renderer_, texture_, nullptr, &picture);
}

void Presenter::present(bool force) {
    if (!renderer_ || !texture_) return;
    const bool changed = panel::generation() != uploaded_generation_;
    if (!changed && !force && !stale_) return;
    upload();
    draw();
    SDL_RenderPresent(renderer_);
    stale_ = false;
    ++frames_;
}

bool Presenter::read_window(std::vector<uint32_t> &pixels, int &width, int &height) {
    if (!renderer_ || !texture_) return false;
    upload();
    draw();
    SDL_Surface *shot = SDL_RenderReadPixels(renderer_, nullptr);
    SDL_Surface *rgb = shot ? SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888) : nullptr;
    if (shot) SDL_DestroySurface(shot);
    const bool ok = rgb != nullptr;
    if (rgb) {
        width = rgb->w;
        height = rgb->h;
        pixels.resize(size_t(width) * size_t(height));
        for (int y = 0; y < height; ++y) {
            const auto *row = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(rgb->pixels) + size_t(y) * size_t(rgb->pitch));
            for (int x = 0; x < width; ++x) pixels[size_t(y) * size_t(width) + size_t(x)] = row[x] & 0xFFFFFF;
        }
        SDL_DestroySurface(rgb);
    }
    SDL_RenderPresent(renderer_);
    stale_ = false;
    ++frames_;
    return ok;
}

bool Presenter::save_window_screenshot(const std::wstring &path) {
    std::vector<uint32_t> pixels;
    int w = 0, h = 0;
    return read_window(pixels, w, h) && write_bmp(path, w, h, pixels.data());
}

const char *Presenter::picture_sampling() const {
    // What the presenter asked for, as SDL keeps it: the software renderer
    // keeps PIXELART although it samples it as nearest, so this pins the
    // choice where the pixels cannot show it.
    SDL_ScaleMode mode = SDL_SCALEMODE_INVALID;
    if (!texture_ || !SDL_GetTextureScaleMode(texture_, &mode)) return "none";
    switch (mode) {
    case SDL_SCALEMODE_NEAREST: return "nearest";
    case SDL_SCALEMODE_PIXELART: return "pixelart";
    case SDL_SCALEMODE_LINEAR: return "linear";
    default: return "none";
    }
}

void Presenter::resize_client(int w, int h) {
    if (!window_) return;
    // A maximised or snapped window is not held to the drag's aspect.
    SDL_SetWindowAspectRatio(window_, 0.0f, 0.0f);
    SDL_SetWindowSize(window_, w, h);
    stale_ = true;
}

void Presenter::set_fullscreen(bool on) {
    if (!window_) return;
    SDL_SetWindowFullscreen(window_, on);
    // The scaling changed while fullscreen: the window takes that mode's shape.
    if (!on && replan_on_leave_ && !fullscreen()) {
        replan_on_leave_ = false;
        set_window_scale(window_scale_);
    }
    stale_ = true;
}

bool Presenter::fullscreen() const {
    return window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
}

bool Presenter::maximized() const {
    return window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0;
}

float Presenter::content_scale() const {
    const float scale = window_ ? SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window_)) : 0.0f;
    return scale > 0.0f ? scale : 1.0f;
}

WorkArea Presenter::work_area() const {
    SDL_Rect r{};
    const SDL_DisplayID display = window_ ? SDL_GetDisplayForWindow(window_) : 0;
    if (display && (SDL_GetDisplayUsableBounds(display, &r) || SDL_GetDisplayBounds(display, &r)) && r.w > 0 && r.h > 0)
        return WorkArea{r.x, r.y, r.w, r.h};
    return WorkArea{0, 0, 1 << 15, 1 << 15}; // unknown: no limit
}

void Presenter::measure_borders() {
    // Only a normal window shows its windowed frame (fullscreen has none; a
    // maximised one hides its sides): otherwise keep the last measurement.
    if (!window_ || (SDL_GetWindowFlags(window_) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)))
        return;
    int top = 0, left = 0, bottom = 0, right = 0;
    if (SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right)) {
        borders_ = WindowBorders{top, left, bottom, right};
        return;
    }
    // Not measurable (SDL could not ask Windows): a caption and a sizing frame
    // at this display scale. Drivers without a frame (offscreen) have none.
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver && std::strcmp(driver, "windows") == 0) {
        const float s = content_scale();
        borders_ = WindowBorders{int(std::lround(31 * s)), int(std::lround(8 * s)), int(std::lround(8 * s)), int(std::lround(8 * s))};
    }
}

uint8_t Presenter::largest_window_scale() {
    measure_borders();
    return uint8_t(uv::largest_window_scale(work_area(), borders_));
}

uint8_t Presenter::default_window_scale() {
    measure_borders();
    return uint8_t(uv::default_window_scale(content_scale(), work_area(), borders_));
}

void Presenter::lock_aspect(const WindowPlan &plan) {
    // While the player drags the frame, the client keeps this window's shape
    // (Windows honours it in WM_SIZING; a maximised or fullscreen window is
    // not held to it).
    const float aspect = float(plan.aspect_w) / float(plan.aspect_h);
    SDL_SetWindowAspectRatio(window_, aspect, aspect);
}

uint8_t Presenter::set_window_scale(uint8_t scale) {
    if (!window_) return 0;
    // Windows ignores a new size while the window is maximised (SDL's
    // WIN_SetWindowSize): restore it first, or the change would be lost.
    if (!fullscreen() && maximized()) {
        SDL_RestoreWindow(window_);
        SDL_SyncWindow(window_);
    }
    measure_borders();
    const WorkArea area = work_area();
    const WindowPlan plan = plan_window(scale, scaling_, area, borders_);
    window_scale_ = uint8_t(plan.scale);
    if (!fullscreen()) {
        // The old aspect lock would bend the new size: lift it, size, place, lock.
        SDL_SetWindowAspectRatio(window_, 0.0f, 0.0f);
        SDL_SetWindowSize(window_, plan.client_w, plan.client_h);
        SDL_SetWindowPosition(window_, plan.x, plan.y);
        lock_aspect(plan);
    }
    ESP_LOGI(kTag, "WINDOW %ux asked, %ux %s client %dx%d at %d,%d (work area %dx%d at %d,%d, frame %d/%d/%d/%d)%s",
             unsigned(scale), unsigned(plan.scale), plan.framed ? "framed" : "bare", plan.client_w, plan.client_h, plan.x,
             plan.y, area.w, area.h, area.x, area.y, borders_.top, borders_.left, borders_.bottom, borders_.right,
             fullscreen() ? " (fullscreen: not applied)" : "");
    stale_ = true;
    return window_scale_;
}

void Presenter::set_scaling(Scaling scaling) {
    scaling_ = scaling;
    stale_ = true;
    if (!window_) return;
    // "Window N x" is a framed client in Framed and a bare one otherwise.
    if (fullscreen()) replan_on_leave_ = true;
    else if (maximized()) lock_aspect(plan_window(window_scale_, scaling_, work_area(), borders_));
    else set_window_scale(window_scale_);
}

void Presenter::set_vsync(bool on) {
    if (renderer_ && !SDL_SetRenderVSync(renderer_, on ? 1 : 0) && on) SDL_SetRenderVSync(renderer_, 0);
}

bool Presenter::save_screenshot(const std::wstring &path) const {
    // The 320x240 panel itself, never the window.
    const uint16_t *src = panel::pixels();
    std::vector<uint32_t> rgb(size_t(kW) * size_t(kH));
    for (size_t i = 0; i < rgb.size(); ++i) {
        const uint16_t p = src[i];
        const uint32_t r = ((p >> 11) & 31) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
        rgb[i] = (r << 16) | (g << 8) | b;
    }
    return write_bmp(path, kW, kH, rgb.data());
}

} // namespace uv
