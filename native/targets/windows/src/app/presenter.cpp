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

void Presenter::present(bool force) {
    if (!renderer_ || !texture_) return;
    const uint64_t generation = panel::generation();
    const uint32_t duty = panel::backlight_duty();
    const bool changed = generation != uploaded_generation_ || duty != applied_duty_;
    if (!changed && !force && !stale_) return;
    if (generation != uploaded_generation_) {
        SDL_UpdateTexture(texture_, nullptr, panel::pixels(), kW * int(sizeof(uint16_t)));
        uploaded_generation_ = generation;
    }
    if (duty != applied_duty_) {
        // The Board clamps Brightness to 10..100 %; the picture is never black.
        const Uint8 level = Uint8(std::clamp<uint32_t>(duty * 255u / 1023u, 26u, 255u));
        SDL_SetTextureColorMod(texture_, level, level, level);
        applied_duty_ = duty;
    }
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    stale_ = false;
    ++frames_;
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
    // A minimal 24-bit BMP of the 320x240 picture (bottom-up rows, BGR).
    const uint16_t *src = panel::pixels();
    const int row_bytes = kW * 3; // 960, already a multiple of 4
    const uint32_t image = uint32_t(row_bytes * kH);
    uint8_t header[54]{};
    auto put32 = [&](int at, uint32_t v) {
        for (int i = 0; i < 4; ++i) header[at + i] = uint8_t(v >> (8 * i));
    };
    header[0] = 'B';
    header[1] = 'M';
    put32(2, 54 + image);
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(kW));
    put32(22, uint32_t(kH));
    header[26] = 1;
    header[28] = 24;
    put32(34, image);
    std::vector<uint8_t> pixels(image);
    for (int y = 0; y < kH; ++y) {
        uint8_t *row = pixels.data() + size_t(kH - 1 - y) * size_t(row_bytes);
        for (int x = 0; x < kW; ++x) {
            const uint16_t p = src[y * kW + x];
            const uint8_t r = uint8_t(((p >> 11) & 31) * 255 / 31), g = uint8_t(((p >> 5) & 63) * 255 / 63),
                          b = uint8_t((p & 31) * 255 / 31);
            row[x * 3] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
    }
    std::FILE *f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(header, 1, sizeof(header), f) == sizeof(header) &&
                    std::fwrite(pixels.data(), 1, pixels.size(), f) == pixels.size();
    return std::fclose(f) == 0 && ok;
}

} // namespace uv
