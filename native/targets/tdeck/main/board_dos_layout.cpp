// Windows port, Original mode: the DOS game's own 320 x 200 game screen on the
// virtual panel's rows 0..199 (Board::set_dos_layout, openu5::platform_dos_screen),
// in place of the handheld's 320 x 240 one. Built only into the desktop target:
// native/targets/windows/CMakeLists.txt lists this file and defines
// OPENU5_DESKTOP_DOS_LAYOUT; the T-Deck firmware compiles none of it, and
// tdeck_board.cpp's handheld screen is the same code it always was.
//
// The geometry is ULTIMA.EXE's frame, 0x63ac..0x64f7 (the web port's
// fiel/frame.ts FRAME_FILLS / FRAME_GLYPHS / FRAME_SEGMENTS), checked pixel by
// pixel against DOSBox-X captures of the player's own install:
//   band (EGA 1)  y 0..6 across; x 0..6 and x 185..191 down to y 191; y 185..191
//                 left of x 192; x 313..319 down to y 87; the panel's bars at
//                 y 57..63 and y 80..87 (x 192..312)
//   corners       IBM.CH 0x7b / 0x7c / 0x7d in the band's blue at cells (0,0),
//                 (39,0) and (0,23), drawn over the band
//   rules (white) the map's box x 7 / x 184, y 7 / y 184; the roster box x 191 /
//                 x 312, y 7 / y 56; the status box y 63 / y 80; the console's
//                 x 191 (y 87..191) and y 87 (x 191..319)
//   map           the 176 x 176 viewport at (8,8), whole: no strip covers it
//   sky band      row 0: sun and moons in cells 6..17 (x 48..143) in RUNES.CH
//   wind band     row 23: >"North Winds"< from col 6 (ULTIMA.EXE 0x2ecb)
//   dungeon bands "L#" on row 0 and "Dir: ..." on row 23, centred on x 96
//   panel         text window 1 (cols 24..39, rows 1..9): the roster on rows
//                 1..6, F/G on row 8, the date centred on row 9; a banner on
//                 row 0 (kernel 0x4e50); the lists draw a DeviceDosPanel there
//   console       text window 2 (cols 24..39, rows 11..23): 13 rows of 16
//                 IBM.CH cells from (192, 88); row 24 (y 192..199) is black
// A band's >< ends are draw_box_edge 0x4c2a / 0x4cce: glyph 0x02 in the band's
// blue with two white lines from (x, y) and (x, y+7) to the tip at col 5, its
// notch black over all eight rows (the rules included).
#include "tdeck_board.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "native_renderer.h"
#include "tdeck_pins.h"

#if !defined(OPENU5_DESKTOP_DOS_LAYOUT)
#error "board_dos_layout.cpp is the desktop target's alone (OPENU5_DESKTOP_DOS_LAYOUT)"
#endif

namespace tdeck {
namespace {
constexpr char kTag[] = "BoardDos";
constexpr int kDisplayWidth = 320, kDisplayHeight = 240, kScreenHeight = 200;
constexpr uint16_t kBlack = 0x0000, kWhite = 0xFFFF, kYellow = 0xFFE0;
constexpr uint16_t kBand = 0x0015; // the handheld frame's band (kChromeBand), EGA 1
constexpr int kMapX = 8, kMapY = 8, kMapSize = openu5::kViewportPixels;
static_assert(kMapSize == 176, "the DOS map is 11 x 11 tiles of 16 px");
constexpr int kTile = openu5::kTilePixels, kTiles = openu5::kViewportTiles;
// Text windows (8 px cells): window 1, the panel; window 2, the console.
constexpr int kPanelX = 191, kPanelW = kDisplayWidth - kPanelX, kPanelH = 88;
constexpr int kWindowX = 24 * 8, kWindowY = 1 * 8;
constexpr int kConsoleX = 24 * 8, kConsoleY = 11 * 8;
constexpr int kBoxRight = 312;
// The bands over the map: row 0 and row 23, x 8..183.
constexpr int kTopBandY = 0, kBottomBandY = 23 * 8, kBandX = kMapX, kBandW = kMapSize;
// The sky track (fiel/skin.ts SKY_COL / SKY_CELLS) and the wind caption's fixed column.
constexpr int kSkyCol = 6, kSkyCells = 12, kWindCol = 6;
// draw_box_edge's white lines (both ends of col 0 included) and glyph 0x02's
// blue: the band ends (over the frame, col 0 keeps it) and the console's echo
// bullet (over black, col 0 is the glyph's).
constexpr uint8_t kEdgeWhite[8] = {0x80, 0x60, 0x18, 0x04, 0x04, 0x18, 0x60, 0x80};
constexpr uint8_t kEdgeBlue[8] = {0x00, 0x00, 0x60, 0x78, 0x78, 0x60, 0x00, 0x00};
constexpr uint8_t kBulletBlue[8] = {0x00, 0x80, 0xe0, 0xf8, 0xf8, 0xe0, 0x80, 0x00};
constexpr char kConsoleBullet = '\x10';
// The frame's corner glyphs as the player's IBM.CH draws them (measured from
// the captures): the stand-ins when the pack has no font.
constexpr uint8_t kCornerTopLeft[8] = {0x07, 0x1f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff};
constexpr uint8_t kCornerTopRight[8] = {0xe0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0xff, 0xff};
constexpr uint8_t kCornerBottomLeft[8] = {0xff, 0xff, 0xff, 0x7f, 0x7f, 0x3f, 0x1f, 0x07};
// The roster row (roster.ts / kernel 0x27ab): name in 9, the arrow, HP in 4, the status letter.
constexpr uint8_t kRosterArrow = 0x1a;

bool font_bit(const uint8_t *font, uint8_t code, int row, int col) {
    return font && (font[size_t(code & 0x7fU) * 8 + size_t(row)] & (0x80U >> unsigned(col))) != 0;
}

/** ULTIMA.EXE's frame at (x, y): its fills, then the corner glyphs, then the white rules. */
uint16_t frame_pixel(int x, int y, const uint8_t *ibm) {
    auto in = [&](int x0, int y0, int x1, int y1) { return x >= x0 && x <= x1 && y >= y0 && y <= y1; };
    if (y >= kScreenHeight) return kBlack;
    uint16_t p = kBlack;
    if (in(0, 0, 319, 6) || in(0, 185, 191, 191) || in(0, 0, 6, 191) || in(185, 0, 191, 191) || in(313, 0, 319, 87) ||
        in(192, 80, 312, 87) || in(192, 57, 312, 63))
        p = kBand;
    auto corner = [&](int cx, int cy, uint8_t code, const uint8_t (&stand_in)[8]) {
        if (!in(cx, cy, cx + 7, cy + 7)) return;
        const int row = y - cy, col = x - cx;
        const bool on = ibm ? font_bit(ibm, code, row, col) : (stand_in[row] & (0x80U >> unsigned(col))) != 0;
        p = on ? kBand : kBlack;
    };
    corner(0, 0, 0x7b, kCornerTopLeft);
    corner(312, 0, 0x7c, kCornerTopRight);
    corner(0, 184, 0x7d, kCornerBottomLeft);
    if ((x == 7 && y >= 7 && y <= 184) || (y == 184 && x >= 7 && x <= 184) || (x == 184 && y >= 7 && y <= 184) ||
        (y == 7 && x >= 7 && x <= 184) || (x == 191 && y >= 87 && y <= 191) || (y == 87 && x >= 191) ||
        (y == 7 && x >= 191 && x <= kBoxRight) || (x == kBoxRight && y >= 7 && y <= 56) ||
        (y == 56 && x >= 191 && x <= kBoxRight) || (x == 191 && y >= 7 && y <= 56) ||
        (y == 63 && x >= 191 && x <= kBoxRight) || (x == kBoxRight && y >= 63 && y <= 80) ||
        (y == 80 && x >= 191 && x <= kBoxRight) || (x == 191 && y >= 63 && y <= 80))
        p = kWhite;
    return p;
}

/** A band end's pixel: `i` is the column in its 8 px cell (mirrored for the closing one), over `base`. */
uint16_t edge_pixel(int i, int row, bool closing, uint16_t base) {
    const unsigned bit = 0x80U >> unsigned(closing ? 7 - i : i);
    if (kEdgeWhite[row] & bit) return kWhite;
    if (kEdgeBlue[row] & bit) return kBand;
    return (closing ? i == 7 : i == 0) ? base : kBlack;
}

/**
 * A notch in a band row that starts at `y0`: the opening end at `start_x`,
 * `cells` black cells after it, the closing end after them. `glyph(cell, row,
 * col)` lights a notch pixel (white, or the colour it returns). Returns
 * `base` outside the notch.
 */
template <class Glyph>
uint16_t notch_pixel(int x, int y, int y0, int start_x, int cells, uint16_t base, Glyph glyph) {
    const int row = y - y0, open_end = start_x, first = start_x + 8, close_end = first + cells * 8;
    if (row < 0 || row >= 8 || cells <= 0) return base;
    if (x >= open_end && x < first) return edge_pixel(x - open_end, row, false, base);
    if (x >= close_end && x < close_end + 8) return edge_pixel(x - close_end, row, true, base);
    if (x >= first && x < close_end) return glyph((x - first) / 8, row, (x - first) % 8);
    return base;
}
} // namespace

template <class PixelAt> esp_err_t Board::paint_rect(int x, int y, int width, int height, PixelAt pixel) {
    if (!display_initialized_ || width <= 0 || height <= 0 || x < 0 || y < 0 || x + width > kDisplayWidth ||
        y + height > kDisplayHeight)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set DOS screen window");
    gpio_set_level(pins::kTftDataCommand, 1);
    const size_t row_length = size_t(width) * 2;
    size_t used = 0;
    RowMark mark{};
    for (int row = 0; row < height; ++row) {
        if (used == 0) mark = row_mark();
        uint8_t *out = transfer_row_.data() + used;
        for (int col = 0; col < width; ++col) {
            const uint16_t p = pixel(x + col, y + row);
            out[col * 2] = uint8_t(p >> 8);
            out[col * 2 + 1] = uint8_t(p);
        }
        used += row_length;
        if (!row_batch_ends(used, row_length, row, height)) continue;
        spi_transaction_t transaction{};
        transaction.length = used * 8;
        transaction.tx_buffer = transfer_row_.data();
        used = 0;
        ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write DOS screen rows");
    }
    debug_last_pixels_ += size_t(width) * size_t(height);
    ++debug_last_dirty_regions_;
    return ESP_OK;
}

void Board::set_dos_layout(bool on) {
    if (on == dos_layout_) return;
    dos_layout_ = on;
    // Either screen repaints from scratch on its next frame.
    alpha_drawn_ = false;
    dos_screen_drawn_ = false;
    console_cursor_.visible = false;
}

int Board::picture_height() const {
    // The handheld's own screens fill the panel: the frontend, the Developer screen.
    if (!dos_layout_ || frontend_drawn_ || debug_drawn_) return kDisplayHeight;
    return kScreenHeight;
}

esp_err_t Board::draw_dos_frame() {
    ++tft_timing_.full_screen;
    return paint_rect(0, 0, kDisplayWidth, kDisplayHeight, [&](int x, int y) { return frame_pixel(x, y, ibm_font_); });
}

esp_err_t Board::draw_dos_band(int y, const char *caption, int start_x, const openu5::HudWorldState *sky,
                               const uint8_t *runes_font) {
    // The frame under it has the band's rule (row 7 of the top band, row 0 of the bottom one).
    const bool track = sky && sky->sky_visible && runes_font;
    const int cells = track ? kSkyCells : caption ? int(std::strlen(caption)) : 0;
    const int first_x = track ? kSkyCol * 8 - 8 : start_x;
    return paint_rect(kBandX, y, kBandW, 8, [&](int x, int py) {
        const uint16_t base = frame_pixel(x, py, ibm_font_);
        return notch_pixel(x, py, y, first_x, cells, base, [&](int cell, int row, int col) -> uint16_t {
            if (track) {
                for (size_t i = 0; i < sky->mark_count; ++i) {
                    if (sky->marks[i].cell != cell) continue;
                    const uint8_t code = sky->marks[i].sun ? 0x2a : sky->marks[i].glyph;
                    if (font_bit(runes_font, code, row, col)) return sky->marks[i].sun ? kYellow : kWhite;
                }
                return kBlack;
            }
            return chrome_glyph_bit(caption[cell], row, col) ? kWhite : kBlack;
        });
    });
}

void Board::compose_dos_world_panel(DeviceDosPanel &out, const openu5::GameState &game,
                                    DevicePartyHighlight party_highlight) const {
    dos_panel_clear(out);
    out.layout = kDosLayoutRosterAndStatus;
    // The roster picker's "Select:" (DS 0x5554, kernel 0x4e50) on the band above the roster.
    if (party_highlight.select_banner) std::snprintf(out.banner, sizeof(out.banner), "Select:");
    // The reverse-video row (openu5::roster_invert_row's order): the damage
    // flash, then the picker's cursor, then the arena's actor.
    const int8_t inverted = party_highlight.damage_flash >= 0 ? party_highlight.damage_flash
                            : party_highlight.cursor >= 0     ? party_highlight.cursor
                                                              : party_highlight.invert_actor;
    const auto members = openu5::party_members(game.party);
    for (size_t row = 0; row < 6 && row < members.count; ++row) {
        const auto index = members.indices[row];
        const auto &a = game.party.characters[index];
        const char status = a.status ? char(a.status) : 'G';
        char line[20]{};
        std::snprintf(line, sizeof(line), "%-9.9s %4u%c", a.name, unsigned(std::min<uint16_t>(a.current_hp, 9999)), status);
        dos_panel_put(out, row, 0, line);
        if (index == game.party.active_character && status != 'D' && status != 'S') out.cells[row][9] = kRosterArrow;
        if (index == inverted) out.inverse[row] = 0x7fff; // cols 0..14, the row's whole 15 cells
    }
    // The status box (panel.ts): F: left, G: right-aligned in 15 cells; the date centred.
    char food[12]{}, gold[12]{}, date[24]{};
    std::snprintf(food, sizeof(food), "F:%u", unsigned(game.food));
    std::snprintf(gold, sizeof(gold), "G:%u", unsigned(game.gold));
    dos_panel_put(out, 7, 0, food);
    const size_t gold_length = std::strlen(gold);
    dos_panel_put(out, 7, gold_length < 15 ? 15 - gold_length : 0, gold);
    std::snprintf(date, sizeof(date), "%ld-%ld-%ld", long(game.time.month), long(game.time.day), long(game.time.year));
    const size_t date_length = std::strlen(date);
    dos_panel_put(out, 8, date_length < 15 ? (15 - date_length) / 2 : 0, date);
}

esp_err_t Board::draw_dos_screen_panel(const DeviceDosPanel &panel, const uint8_t *runes_font) {
    const bool split = panel.layout == kDosLayoutRosterAndStatus;
    // The banner, centred in the window's 15 cells; the arrow at screen col 31,
    // in the band under the roster box (split) or under the box (row 10).
    const int banner_cells = int(std::min(std::strlen(panel.banner), kDosPanelCols));
    const int banner_x = kWindowX + std::max(0, (15 - (banner_cells + 2)) / 2) * 8;
    const int arrow_y = split ? 7 * 8 : 10 * 8;
    auto glyph_on = [&](uint8_t code, bool runes, int row, int col) {
        const uint8_t *font = runes && runes_font ? runes_font : ibm_font_;
        return font ? font_bit(font, code, row, col) : chrome_glyph_bit(char(code), row, col);
    };
    return paint_rect(kPanelX, 0, kPanelW, kPanelH, [&](int x, int y) -> uint16_t {
        uint16_t base = frame_pixel(x, y, ibm_font_);
        // One box (the lists): window 1 cleared inside (putchar 0xff, cols
        // 24..38 x rows 1..9) and its sides joined across the bar between the
        // frame's two boxes (y 57..62).
        if (!split && y >= 57 && y <= 62 && (x == 191 || x == kBoxRight)) base = kWhite;
        if (y < 8) {
            if (!banner_cells) return base;
            return notch_pixel(x, y, 0, banner_x, banner_cells, base, [&](int cell, int row, int col) -> uint16_t {
                return glyph_on(uint8_t(panel.banner[cell]), false, row, col) ? kWhite : kBlack;
            });
        }
        if (y >= arrow_y && y < arrow_y + 8 && panel.arrow) {
            const uint16_t p = notch_pixel(x, y, arrow_y, 30 * 8, 1, base, [&](int, int row, int col) -> uint16_t {
                return glyph_on(panel.arrow, false, row, col) ? kWhite : kBlack;
            });
            if (x >= 30 * 8 && x < 33 * 8) return p;
        }
        const int row = (y - kWindowY) / 8;
        const bool window_row = y >= kWindowY && y < kWindowY + int(kDosPanelRows) * 8 && !(split && row == 6);
        if (!window_row || x < kWindowX) return (!split && y >= 8 && y < 80 && x > 191 && x < kBoxRight) ? kBlack : base;
        const int col = (x - kWindowX) / 8;
        const uint8_t code = panel.cells[row][col];
        // Col 15 is the box's rule and the band unless something is printed there.
        if (col == 15 && code == kDosBlank && !((panel.inverse[row] >> 15) & 1U)) return base;
        const bool on = glyph_on(code, (panel.runes[row] >> col) & 1U, (y - kWindowY) % 8, (x - kWindowX) % 8);
        return on != (((panel.inverse[row] >> col) & 1U) != 0) ? kWhite : kBlack;
    });
}

esp_err_t Board::draw_dos_console(const openu5::UiSession &ui, bool force) {
    auto &cache = *dos_cache_;
    openu5::UiRenderedLine lines[kDosConsoleRows]{};
    const size_t count = ui.visible_lines(lines, kDosConsoleRows, kDosConsoleColumns);
    // The text window fills upward from its last row (fiel/console.ts anchorBottom).
    const size_t top = ui.console_layout() && count < kDosConsoleRows ? kDosConsoleRows - count : 0;
    char rows[kDosConsoleRows][openu5::kUiRenderedLineBytes + 1]{};
    bool command[kDosConsoleRows]{};
    for (size_t i = 0; i < kDosConsoleRows; ++i) {
        if (i < top || i - top >= count) continue;
        const auto &line = lines[i - top];
        command[i] = (line.flags & openu5::UiTextCommand) != 0;
        if (command[i]) {
            rows[i][0] = kConsoleBullet;
            std::snprintf(rows[i] + 1, sizeof(rows[i]) - 1, "%s", line.text);
        } else {
            std::snprintf(rows[i], sizeof(rows[i]), "%s", line.text);
        }
    }
    // The wait cursor (poll_key_blink_cursor 0x1b38) after the newest row's text, when it fits.
    ConsoleCursorState cursor{};
    if (count && ibm_font_ && ui.scroll_offset_lines() == 0 && ui.console_cursor() != openu5::UiConsoleCursor::None) {
        const size_t last = top + count - 1, column = std::strlen(rows[last]);
        if (column < kDosConsoleColumns) {
            cursor.x = int16_t(kConsoleX + int(column) * 8);
            cursor.y = int16_t(kConsoleY + int(last) * 8);
            cursor.width = 8;
            cursor.height = 8;
            cursor.visible = true;
        }
    }
    const bool cursor_moved = console_cursor_.visible && (!cursor.visible || cursor.x != console_cursor_.x ||
                                                          cursor.y != console_cursor_.y);
    // Rows that keep their text are not redrawn: a cursor that left one is erased.
    if (cursor_moved && !force)
        ESP_RETURN_ON_ERROR(fill_rect(console_cursor_.x, console_cursor_.y, console_cursor_.width, console_cursor_.height, kBlack),
                            kTag, "erase console cursor");
    bool cursor_row_drawn = false;
    for (size_t i = 0; i < kDosConsoleRows; ++i) {
        if (!force && cache.console_valid && std::strcmp(cache.console[i], rows[i]) == 0) continue;
        const int y0 = kConsoleY + int(i) * 8;
        const char *text = rows[i];
        const size_t length = std::strlen(text);
        const bool bullet = command[i];
        ESP_RETURN_ON_ERROR(paint_rect(kConsoleX, y0, int(kDosConsoleColumns) * 8, 8,
                                       [&](int x, int y) -> uint16_t {
                                           const size_t col = size_t(x - kConsoleX) / 8;
                                           const int gx = (x - kConsoleX) % 8, gy = y - y0;
                                           if (col >= length) return kBlack;
                                           if (col == 0 && bullet) {
                                               const unsigned bit = 0x80U >> unsigned(gx);
                                               return (kEdgeWhite[gy] & bit) ? kWhite : (kBulletBlue[gy] & bit) ? kBand : kBlack;
                                           }
                                           return chrome_glyph_bit(text[col], gy, gx) ? kWhite : kBlack;
                                       }),
                            kTag, "draw console row");
        std::snprintf(cache.console[i], sizeof(cache.console[i]), "%s", text);
        cursor_row_drawn = cursor_row_drawn || y0 == cursor.y;
    }
    cache.console_valid = true;
    if (!cursor.visible) {
        console_cursor_.visible = false;
    } else if (force || cursor_row_drawn || cursor_moved || !console_cursor_.visible || console_cursor_.phase != console_phase_) {
        console_cursor_ = cursor;
        ESP_RETURN_ON_ERROR(draw_console_cursor_cell(), kTag, "draw console cursor");
    }
    return ESP_OK;
}

esp_err_t Board::show_alpha_dos(const uint16_t *pixels, const openu5::UiSession &ui, const openu5::GameState &game,
                                const openu5::HudWorldState &hud, const uint8_t *runes_font, const uint8_t *animated_cells,
                                bool animation_only, const DeviceSelectionView *selection,
                                DevicePartyHighlight party_highlight, uint32_t viewport_crc,
                                const openu5::HudDungeonBands *dungeon_bands, bool preserve_party_panel) {
    debug_last_full_redraw_ = false;
    debug_last_dirty_regions_ = 0;
    debug_last_pixels_ = 0;
    endgame_page_key_ = -1;
    if (!dos_cache_) dos_cache_.reset(new DosScreenCache());
    auto &cache = *dos_cache_;
    // Anything else that painted the panel (the frontend, the Developer
    // screen, an ending page, a layout switch) leaves the frame to redraw.
    const bool full = !alpha_drawn_ || frontend_drawn_ || debug_drawn_ || !dos_screen_drawn_;
    if (full) {
        ESP_RETURN_ON_ERROR(draw_dos_frame(), kTag, "draw the DOS frame");
        alpha_drawn_ = true;
        frontend_drawn_ = false;
        debug_drawn_ = false;
        debug_cache_valid_ = false;
        dos_screen_drawn_ = true;
        cache = DosScreenCache{};
        console_cursor_.visible = false;
        animation_only = false;
        debug_last_full_redraw_ = true;
        // The handheld screen's caches: whatever they knew is gone too.
        alpha_ui_cache_valid_ = false;
        viewport_cache_valid_ = false;
        sky_bar_cache_valid_ = false;
        shop_cache_valid_ = false;
        selection_cache_valid_ = false;
        dos_panel_cache_valid_ = false;
        context_cache_valid_ = false;
    }
    if (animation_only && animated_cells) {
        // A run of adjacent animated cells in a tile row is one window; nothing clips them here.
        for (int row = 0; row < kTiles; ++row)
            for (int col = 0; col < kTiles;) {
                if (!animated_cells[row * kTiles + col]) {
                    ++col;
                    continue;
                }
                int end = col + 1;
                while (end < kTiles && animated_cells[row * kTiles + end]) ++end;
                ESP_RETURN_ON_ERROR(draw_rgb565_strided(kMapX + col * kTile, kMapY + row * kTile, (end - col) * kTile, kTile,
                                                        pixels + row * kTile * kMapSize + col * kTile, kMapSize),
                                    kTag, "draw animated cells");
                col = end;
            }
        if (console_cursor_.visible && console_cursor_.phase != console_phase_)
            ESP_RETURN_ON_ERROR(draw_console_cursor_cell(), kTag, "animate console cursor");
        return ESP_OK;
    }
    // The map, whole.
    if (!cache.viewport_valid || cache.viewport_crc != viewport_crc) {
        ++tft_timing_.viewport_full;
        ESP_RETURN_ON_ERROR(draw_rgb565(kMapX, kMapY, kMapSize, kMapSize, pixels), kTag, "draw the map");
        debug_last_pixels_ += size_t(kMapSize) * kMapSize;
        ++debug_last_dirty_regions_;
        cache.viewport_crc = viewport_crc;
        cache.viewport_valid = true;
        viewport_cache_valid_ = true;
    }
    // The bands: the sky (or the dungeon's level) on row 0, the wind (or the
    // dungeon's facing) on row 23. A caption is keyed by its text, the sky by its marks.
    const bool bands = dungeon_bands && dungeon_bands->active;
    uint32_t sky_signature = bands ? 0x9dU : hud.sky_visible ? 0x51U : 0x17U;
    if (bands)
        for (const char *c = dungeon_bands->level; *c; ++c) sky_signature = sky_signature * 16777619U ^ uint32_t(uint8_t(*c));
    else
        for (size_t i = 0; i < hud.mark_count; ++i)
            sky_signature = sky_signature * 16777619U ^ (uint32_t(hud.marks[i].cell) << 16 | uint32_t(hud.marks[i].glyph) << 8 |
                                                         uint32_t(hud.marks[i].sun));
    auto centred_x = [](const char *text) {
        // fiel/skin.ts drawCenteredBand: >text< centred on the map's centre, x 96.
        const int band = (int(std::strlen(text)) + 2) * 8;
        return 96 - band / 2;
    };
    if (!cache.sky_valid || cache.sky_signature != sky_signature) {
        if (bands)
            ESP_RETURN_ON_ERROR(draw_dos_band(kTopBandY, dungeon_bands->level, centred_x(dungeon_bands->level), nullptr, nullptr),
                                kTag, "draw the level band");
        else
            ESP_RETURN_ON_ERROR(draw_dos_band(kTopBandY, nullptr, 0, hud.sky_visible ? &hud : nullptr, runes_font), kTag,
                                "draw the sky band");
        ++draw_calls_.sky_strips;
        cache.sky_signature = sky_signature;
        cache.sky_valid = true;
    }
    char bottom[34]{};
    if (bands) std::snprintf(bottom, sizeof(bottom), "%.30s", dungeon_bands->direction);
    else if (hud.wind_visible) std::snprintf(bottom, sizeof(bottom), "%.30s", hud.wind);
    if (!cache.bottom_valid || std::strcmp(cache.bottom_band, bottom) != 0) {
        const int start_x = bands ? centred_x(bottom) : kWindCol * 8;
        ESP_RETURN_ON_ERROR(draw_dos_band(kBottomBandY, bottom[0] ? bottom : nullptr, start_x, nullptr, nullptr), kTag,
                            "draw the wind band");
        std::snprintf(cache.bottom_band, sizeof(cache.bottom_band), "%s", bottom);
        cache.bottom_valid = true;
    }
    // The right panel: a list's own (the 1988 interaction model), else the
    // frame's roster and status boxes. A list the runtime has no DOS form for
    // is shown in the list frame all the same, under its title.
    DeviceDosPanel world{};
    const DeviceDosPanel *panel = nullptr;
    if (selection && selection->active && selection->dos) {
        panel = selection->dos;
    } else if (selection && selection->active) {
        dos_panel_clear(world);
        std::snprintf(world.banner, sizeof(world.banner), "%.15s", selection->title);
        constexpr size_t kRows = kDosPanelRows - 2;
        dos_panel_list_frame(world, kRows);
        for (size_t i = 0; i < selection->row_count && i < kRows; ++i) {
            const char *text = selection->rows[i];
            while (*text == ' ') ++text;
            char cell_text[14]{};
            std::snprintf(cell_text, sizeof(cell_text), "%.13s", text);
            dos_panel_put(world, i + 1, 1, cell_text);
            if (i == selection->selected_row) world.inverse[i + 1] = 0x3ffe;
        }
        world.arrow = dos_panel_arrow(selection->page_start, kRows, selection->total);
        panel = &world;
    } else if (!(preserve_party_panel && cache.panel_valid)) {
        compose_dos_world_panel(world, game, party_highlight);
        panel = &world;
    }
    if (panel && (!cache.panel_valid || std::memcmp(&cache.panel, panel, sizeof(DeviceDosPanel)) != 0)) {
        ESP_RETURN_ON_ERROR(draw_dos_screen_panel(*panel, runes_font), kTag, "draw the panel");
        cache.panel = *panel;
        cache.panel_valid = true;
    }
    return draw_dos_console(ui, full);
}

esp_err_t Board::dos_camp_viewport(const uint16_t *pixels) {
    const auto result = draw_rgb565(kMapX, kMapY, kMapSize, kMapSize, pixels);
    if (result == ESP_OK && dos_cache_) dos_cache_->viewport_valid = false;
    viewport_cache_valid_ = false;
    return result;
}

esp_err_t Board::dos_fill_bed_viewport() {
    // CMDS: the whole 176 x 176 game window (8,8)-(183,183), as the original.
    const auto result = fill_rect(kMapX, kMapY, kMapSize, kMapSize, kBlack);
    if (result == ESP_OK && dos_cache_) dos_cache_->viewport_valid = false;
    viewport_cache_valid_ = false;
    return result;
}

esp_err_t Board::dos_refresh_bed_status_panel(const openu5::GameState &game, DevicePartyHighlight party_highlight) {
    if (!dos_cache_ || !dos_screen_drawn_) return ESP_OK; // the next frame draws the whole screen
    DeviceDosPanel world{};
    compose_dos_world_panel(world, game, party_highlight);
    ESP_RETURN_ON_ERROR(draw_dos_screen_panel(world, nullptr), kTag, "refresh the bed panel");
    dos_cache_->panel = world;
    dos_cache_->panel_valid = true;
    return ESP_OK;
}

} // namespace tdeck
