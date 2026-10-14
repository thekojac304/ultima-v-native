#pragma once
// Ultima V Native for Windows: the window and the picture.
//
// The virtual panel (src/platform/virtual_panel.h) holds the game's 320x240
// RGB565 screen. The presenter owns the SDL window, renderer and one
// streaming texture of that size, uploads the panel when it changed and
// draws it scaled with nearest-neighbour sampling, keeping the 4:3 aspect:
// whole-number scales (Scaling::Integer, letterboxed) or the largest fit
// (Scaling::Fit). The panel's backlight duty (the T-Deck's Brightness) is
// ignored: the picture is always drawn at full intensity.
#include <cstdint>
#include <string>

#include "app/desktop_settings.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace uv {

class Presenter {
  public:
    Presenter() = default;
    Presenter(const Presenter &) = delete;
    Presenter &operator=(const Presenter &) = delete;
    ~Presenter();

    /** Creates the window (hidden until show()), renderer and texture. False with error() set. */
    bool create(const char *title, const DesktopSettings &settings);
    void destroy();
    void show();
    bool created() const { return window_ != nullptr; }

    /** Upload the panel if it changed (or `force`), draw and present. */
    void present(bool force = false);
    /** The window was exposed/resized/restored: present on the next call even if nothing changed. */
    void invalidate() { stale_ = true; }

    void set_fullscreen(bool on);
    bool fullscreen() const;
    /** Windowed size = 320x240 x scale (clamped to what fits the display). */
    void set_window_scale(uint8_t scale);
    void set_scaling(Scaling scaling);
    void set_vsync(bool on);

    /** Writes the current panel picture as a 24-bit BMP (the user's own screenshot). */
    bool save_screenshot(const std::wstring &path) const;

    SDL_Window *window() const { return window_; }
    const std::string &error() const { return error_; }
    uint64_t frames_presented() const { return frames_; }

  private:
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    uint64_t uploaded_generation_ = ~0ull;
    uint64_t frames_ = 0;
    bool stale_ = true;
    Scaling scaling_ = Scaling::Integer;
    std::string error_;
};

} // namespace uv
