#include "app/presenter.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <SDL3/SDL.h>

#include "platform/virtual_panel.h"

namespace uv {
namespace {
constexpr int kW = panel::kWidth, kH = panel::kHeight;

SDL_RendererLogicalPresentation presentation_mode(Scaling scaling) {
    return scaling == Scaling::Fit ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
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
    const int scale = std::clamp<int>(settings.window_scale, kMinWindowScale, kMaxWindowScale);
    window_ = SDL_CreateWindow(title, kW * scale, kH * scale,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
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
    if (!texture_) {
        error_ = std::string("SDL_CreateTexture: ") + SDL_GetError();
        destroy();
        return false;
    }
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    scaling_ = settings.scaling;
    SDL_SetRenderLogicalPresentation(renderer_, kW, kH, presentation_mode(scaling_));
    set_vsync(settings.vsync);
    set_window_scale(settings.window_scale);
    if (settings.fullscreen) SDL_SetWindowFullscreen(window_, true);
    stale_ = true;
    return true;
}

void Presenter::destroy() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
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
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
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
    // The logical presentation limits a read to the picture: read the whole output.
    SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_Surface *shot = SDL_RenderReadPixels(renderer_, nullptr);
    SDL_SetRenderLogicalPresentation(renderer_, kW, kH, presentation_mode(scaling_));
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

void Presenter::resize_client(int w, int h) {
    if (!window_) return;
    SDL_SetWindowSize(window_, w, h);
    stale_ = true;
}

void Presenter::set_fullscreen(bool on) {
    if (!window_) return;
    SDL_SetWindowFullscreen(window_, on);
    stale_ = true;
}

bool Presenter::fullscreen() const {
    return window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Presenter::set_window_scale(uint8_t scale) {
    if (!window_) return;
    int s = std::clamp<int>(scale, kMinWindowScale, kMaxWindowScale);
    // Never larger than the display's usable area.
    SDL_Rect usable{};
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (display && SDL_GetDisplayUsableBounds(display, &usable)) {
        while (s > 1 && (kW * s > usable.w || kH * s > usable.h - 32)) --s;
    }
    if (!fullscreen()) {
        SDL_SetWindowSize(window_, kW * s, kH * s);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    stale_ = true;
}

void Presenter::set_scaling(Scaling scaling) {
    scaling_ = scaling;
    if (renderer_) SDL_SetRenderLogicalPresentation(renderer_, kW, kH, presentation_mode(scaling_));
    stale_ = true;
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
