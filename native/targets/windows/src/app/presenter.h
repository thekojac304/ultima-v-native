#pragma once
// Ultima V Native for Windows: the window and the picture.
//
// The virtual panel (src/platform/virtual_panel.h) holds the game's 320x240
// RGB565 screen. The presenter owns the SDL window, renderer and one
// streaming texture of that size, uploads the panel when it changed and
// draws it where app/presentation_layout.h puts it, in whole output pixels
// (SDL's logical presentation is off): whole-number scales with nearest
// sampling and the Classic Framed ring (Scaling::Framed, the default) or
// without it (Scaling::Integer), or the largest 4:3 fit sampled as pixel art
// (Scaling::Fit). The ring's colours come from a 2x1 RGB565 texture, so they
// take the picture's own colour path. The panel's backlight duty (the
// T-Deck's Brightness) is ignored: the picture is always drawn at full
// intensity.
#include <cstdint>
#include <string>
#include <vector>

#include "app/desktop_settings.h"
#include "app/presentation_layout.h"

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

    /** Borderless fullscreen at the desktop's own mode (never an exclusive mode change). */
    void set_fullscreen(bool on);
    bool fullscreen() const;
    bool maximized() const;
    /**
     * "Window N x" (plan_window): the picture at N in a framed client when it
     * fits the work area with the window's real frame, else bare, else a
     * smaller N; restored first if maximised, centred on its display with its
     * title bar inside the work area, its aspect locked for dragging. Returns
     * the N it got (the caller shows and saves that, never the request).
     */
    uint8_t set_window_scale(uint8_t scale);
    /** The N the window has now. */
    uint8_t window_scale() const { return window_scale_; }
    /** The largest N that fits the window's display now. */
    uint8_t largest_window_scale();
    /** The N for a player who has not chosen one: from the display's scale (default_window_scale). */
    uint8_t default_window_scale();
    /** The scaling; a normal window takes the mode's shape (framed or bare client). */
    void set_scaling(Scaling scaling);
    void set_vsync(bool on);

    /** Writes the current panel picture as a 24-bit BMP (the user's own screenshot). */
    bool save_screenshot(const std::wstring &path) const;

    // --autotest: the window itself, as the player sees it (F12 stays the panel).
    /** Draws the current frame, reads the whole output back as 0xRRGGBB pixels, then presents it. */
    bool read_window(std::vector<uint32_t> &pixels, int &width, int &height);
    /** read_window() as a 24-bit BMP. */
    bool save_window_screenshot(const std::wstring &path);
    /** Sets the window's client area to w x h, as a drag, a snap or a maximise does. */
    void resize_client(int w, int h);

    SDL_Window *window() const { return window_; }
    const std::string &error() const { return error_; }
    uint64_t frames_presented() const { return frames_; }
    /** The layout of the last frame drawn. */
    const PresentationLayout &layout() const { return layout_; }

  private:
    /** Uploads the panel when it changed since the last upload. */
    void upload();
    /** Clears and draws the frame into the back buffer (no present). */
    void draw();
    float content_scale() const;
    WorkArea work_area() const;
    /** The window's frame, measured while it is a normal window (kept otherwise). */
    void measure_borders();
    void lock_aspect(const WindowPlan &plan);

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    SDL_Texture *ring_ = nullptr; // 2x1 RGB565: the band's blue, the rule's white
    int texture_scale_mode_ = 0;  // the picture texture's SDL_ScaleMode
    uint64_t uploaded_generation_ = ~0ull;
    uint64_t frames_ = 0;
    bool stale_ = true;
    Scaling scaling_ = Scaling::Framed;
    uint8_t window_scale_ = 3;
    WindowBorders borders_{};
    bool replan_on_leave_ = false; // the scaling changed while fullscreen
    PresentationLayout layout_{};
    std::string error_;
};

} // namespace uv
