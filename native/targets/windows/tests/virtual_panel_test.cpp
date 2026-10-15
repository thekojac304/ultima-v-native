// Ultima V Native for Windows -- the virtual ST7789 (src/platform/virtual_panel.*).
//
// Part 1 drives the ESP-IDF driver calls the Board makes (spi_bus_initialize,
// spi_bus_add_device, gpio_set_level on the D/C pin, spi_device_transmit,
// ledc_set_duty) by hand with CASET/RASET/RAMWR sequences and pixel payloads,
// and checks the decoded panel memory pixel by pixel.
//
// Part 2 runs the REAL shared tdeck::Board (native/targets/tdeck/main/
// tdeck_board.cpp, unchanged) on that panel: initialize_display(), then
// show_frontend() with a hand-built openu5::FrontendView, and prints the
// panel's FNV-1a hash so a GCC build of the same Board can be compared.
//
// Panel hash definition (both parts): FNV-1a 64 over the 320x240 pixels in
// row-major order, each pixel as two bytes, HIGH byte first (the ST7789 wire
// order), offset basis 0xcbf29ce484222325, prime 0x100000001b3.
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <vector>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "openu5/frontend.h"
#include "openu5/hud.h"
#include "openu5/state.h"
#include "openu5/turn.h"
#include "openu5/ui_session.h"
#include "platform/virtual_panel.h"
#include "tdeck_board.h"
#include "tdeck_pins.h"
#include "test_check.h"

using uvtest::check;
using uvtest::format;
namespace panel = uv::panel;

namespace {

spi_device_handle_t g_handle = nullptr;

uint64_t panel_hash() {
    uvtest::Fnv1a64 f;
    const uint16_t *p = panel::pixels();
    for (int i = 0; i < panel::kWidth * panel::kHeight; ++i) {
        f.byte(uint8_t(p[i] >> 8));
        f.byte(uint8_t(p[i]));
    }
    return f.h;
}

uint16_t at(int x, int y) { return panel::pixels()[size_t(y) * panel::kWidth + size_t(x)]; }

size_t count_not(uint16_t value) {
    size_t n = 0;
    const uint16_t *p = panel::pixels();
    for (int i = 0; i < panel::kWidth * panel::kHeight; ++i) n += p[i] != value;
    return n;
}

esp_err_t cmd(uint8_t command) {
    gpio_set_level(tdeck::pins::kTftDataCommand, 0);
    spi_transaction_t t{};
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = command;
    return spi_device_transmit(g_handle, &t);
}

esp_err_t send(const std::vector<uint8_t> &bytes) {
    gpio_set_level(tdeck::pins::kTftDataCommand, 1);
    spi_transaction_t t{};
    t.length = bytes.size() * 8;
    t.tx_buffer = bytes.data();
    return spi_device_transmit(g_handle, &t);
}

std::vector<uint8_t> be(std::initializer_list<uint16_t> pixels) {
    std::vector<uint8_t> out;
    for (uint16_t p : pixels) {
        out.push_back(uint8_t(p >> 8));
        out.push_back(uint8_t(p));
    }
    return out;
}

void window(int x0, int y0, int x1, int y1) {
    cmd(0x2A);
    send({uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)});
    cmd(0x2B);
    send({uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)});
}

void driver_level() {
    panel::reset(0x1234);
    bool filled = true;
    for (int i = 0; i < panel::kWidth * panel::kHeight; ++i) filled = filled && panel::pixels()[i] == 0x1234;
    check(filled && panel::stats().transactions == 0 && panel::stats().malformed == 0,
          "V1 reset(0x1234) fills all 320x240 pixels and clears the statistics");

    spi_bus_config_t bus{};
    bus.max_transfer_sz = panel::kWidth * 2; // what the Board configures (640)
    check(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) == ESP_OK, "V2 spi_bus_initialize (max_transfer_sz 640)");
    spi_device_interface_config_t dev{};
    check(spi_bus_add_device(SPI2_HOST, &dev, &g_handle) == ESP_OK && g_handle != nullptr,
          "V3 spi_bus_add_device returns a device handle");
    check(spi_bus_add_device(SPI2_HOST, &dev, nullptr) == ESP_ERR_INVALID_ARG, "V3 spi_bus_add_device(null handle): invalid argument");
    spi_transaction_t t{};
    check(spi_device_transmit(nullptr, &t) == ESP_ERR_INVALID_ARG && spi_device_transmit(g_handle, nullptr) == ESP_ERR_INVALID_ARG,
          "V4 spi_device_transmit with a null handle or transaction: invalid argument");

    // A 3x2 window at (10,20).
    const uint64_t gen0 = panel::generation();
    window(10, 20, 12, 21);
    check(panel::generation() == gen0, "V5 CASET/RASET (commands + parameters) do not count as pixel writes");
    cmd(0x2C);
    check(panel::generation() == gen0 && panel::stats().windows == 1, "V6 RAMWR alone writes nothing (and counts one window)");
    send(be({0xA001, 0xA002, 0xA003, 0xA004, 0xA005, 0xA006}));
    check(panel::generation() == gen0 + 1, "V7 a pixel transaction increments generation() by one");
    const bool placed = at(10, 20) == 0xA001 && at(11, 20) == 0xA002 && at(12, 20) == 0xA003 && at(10, 21) == 0xA004 &&
                        at(11, 21) == 0xA005 && at(12, 21) == 0xA006;
    check(placed, "V8 six pixels fill the 3x2 window row by row: (10..12,20) then (10..12,21)",
          format("%04x %04x %04x / %04x %04x %04x", at(10, 20), at(11, 20), at(12, 20), at(10, 21), at(11, 21), at(12, 21)));
    check(at(9, 20) == 0x1234 && at(13, 20) == 0x1234 && at(10, 19) == 0x1234 && at(10, 22) == 0x1234 && at(13, 21) == 0x1234,
          "V9 nothing outside the window changed");

    // Past the window's end the pointer wraps to its start.
    send(be({0xB001, 0xB002}));
    check(at(10, 20) == 0xB001 && at(11, 20) == 0xB002 && at(12, 20) == 0xA003,
          "V10 the 7th and 8th pixels wrap to the window's start (10,20), (11,20)",
          format("%04x %04x %04x", at(10, 20), at(11, 20), at(12, 20)));
    // RAMWR returns the pointer to the window's start.
    cmd(0x2C);
    send(be({0xC001}));
    check(at(10, 20) == 0xC001 && at(11, 20) == 0xB002, "V11 RAMWR resets the write pointer to the window's start");
    // Successive pixel transactions continue where the last one stopped.
    send(be({0xC002}));
    send(be({0xC003, 0xC004}));
    check(at(11, 20) == 0xC002 && at(12, 20) == 0xC003 && at(10, 21) == 0xC004,
          "V12 pixels split over several transactions continue the same pointer");

    // Byte order: big-endian RGB565.
    window(0, 0, 0, 0);
    cmd(0x2C);
    send({0xAB, 0xCD});
    check(at(0, 0) == 0xABCD, "V13 pixel bytes are big-endian: {0xAB, 0xCD} is 0xABCD", format("got %04x", at(0, 0)));

    // A 4-byte pixel payload in tx_data (SPI_TRANS_USE_TXDATA).
    window(30, 30, 31, 30);
    cmd(0x2C);
    {
        gpio_set_level(tdeck::pins::kTftDataCommand, 1);
        spi_transaction_t tx{};
        tx.flags = SPI_TRANS_USE_TXDATA;
        tx.length = 32;
        tx.tx_data[0] = 0x11;
        tx.tx_data[1] = 0x22;
        tx.tx_data[2] = 0x33;
        tx.tx_data[3] = 0x44;
        spi_device_transmit(g_handle, &tx);
    }
    check(at(30, 30) == 0x1122 && at(31, 30) == 0x3344, "V14 pixels carried in tx_data (USE_TXDATA) are decoded too");

    // CASET parameters split over two data transactions.
    cmd(0x2A);
    send({0x00, 50});
    send({0x00, 51});
    cmd(0x2B);
    send({0x00, 60, 0x00, 60});
    cmd(0x2C);
    send(be({0xD001, 0xD002, 0xD003}));
    // Window x 50..51, y 60..60: the 3rd pixel wraps back onto (50,60).
    check(at(50, 60) == 0xD003 && at(51, 60) == 0xD002 && at(52, 60) == 0x1234 && at(50, 61) == 0x1234,
          "V15 CASET parameters split 2+2 bytes still set the window; a 2x1 window wraps its 3rd pixel onto (50,60)",
          format("%04x %04x %04x %04x", at(50, 60), at(51, 60), at(52, 60), at(50, 61)));

    // Non-pixel commands and their parameters leave the memory alone.
    const uint64_t h_before = panel_hash();
    const uint64_t g_before = panel::generation();
    cmd(0x36);
    send({0x60});
    cmd(0x3A);
    send({0x55});
    cmd(0x21);
    cmd(0x11);
    cmd(0xE0);
    send({0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38, 0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30});
    check(panel_hash() == h_before && panel::generation() == g_before,
          "V17 MADCTL/COLMOD/INVON/SLPOUT/gamma and their data do not touch the panel memory");

    // max_transfer_sz: the driver refuses longer transactions, so does the panel.
    window(0, 100, panel::kWidth - 1, 101);
    cmd(0x2C);
    const uint64_t malformed0 = panel::stats().malformed;
    std::vector<uint8_t> row(size_t(panel::kWidth) * 2, 0x5A);
    std::vector<uint8_t> too_long(size_t(panel::kWidth) * 2 + 1, 0x77);
    const uint64_t g1 = panel::generation();
    const esp_err_t refused = send(too_long);
    check(refused == ESP_ERR_INVALID_ARG && panel::stats().malformed == malformed0 + 1 && panel::generation() == g1 &&
              at(0, 100) == 0x1234,
          "V18 a 641-byte transaction (> max_transfer_sz 640) is refused, counted malformed, and writes nothing",
          format("err=%d", int(refused)));
    const esp_err_t accepted = send(row);
    bool row_ok = accepted == ESP_OK;
    for (int x = 0; x < panel::kWidth; ++x) row_ok = row_ok && at(x, 100) == 0x5A5A;
    check(row_ok && at(0, 101) == 0x1234, "V19 exactly 640 bytes (one full row) is accepted and fills row 100");

    // An odd byte count: whole pixels are written, the stray byte is malformed.
    window(200, 200, 203, 200);
    cmd(0x2C);
    const uint64_t malformed1 = panel::stats().malformed;
    send({0x01, 0x02, 0x03, 0x04, 0x05});
    check(at(200, 200) == 0x0102 && at(201, 200) == 0x0304 && at(202, 200) == 0x1234 && panel::stats().malformed == malformed1 + 1,
          "V20 a 5-byte payload writes two pixels and counts one malformed transaction");

    // Writes outside the panel are dropped (and counted), not wrapped into the next row.
    window(318, 150, 321, 151);
    cmd(0x2C);
    const uint64_t malformed2 = panel::stats().malformed;
    send(be({0xE001, 0xE002, 0xE003, 0xE004, 0xE005}));
    check(at(318, 150) == 0xE001 && at(319, 150) == 0xE002 && at(0, 151) == 0x1234 && at(318, 151) == 0xE005 &&
              panel::stats().malformed == malformed2 + 2,
          "V21 a window reaching past x 319 drops the off-panel pixels (2 malformed) and continues on its next row",
          format("(318,151)=%04x (0,151)=%04x malformed+%llu", at(318, 151), at(0, 151),
                 (unsigned long long)(panel::stats().malformed - malformed2)));

    // A command transaction must be exactly one byte.
    {
        const uint64_t m = panel::stats().malformed;
        gpio_set_level(tdeck::pins::kTftDataCommand, 0);
        const uint8_t two[2] = {0x2C, 0x00};
        spi_transaction_t tx{};
        tx.length = 16;
        tx.tx_buffer = two;
        spi_device_transmit(g_handle, &tx);
        check(panel::stats().malformed == m + 1, "V22 a 2-byte command transaction is counted malformed");
    }

    // A full-screen fill reproduces exactly a reset() panel.
    window(0, 0, panel::kWidth - 1, panel::kHeight - 1);
    cmd(0x2C);
    std::vector<uint8_t> fill(size_t(panel::kWidth) * 2);
    for (size_t i = 0; i < fill.size(); i += 2) {
        fill[i] = 0xF8;
        fill[i + 1] = 0x1F;
    }
    for (int y = 0; y < panel::kHeight; ++y) send(fill);
    const uint64_t filled_hash = panel_hash();
    panel::reset(0xF81F);
    check(filled_hash == panel_hash() && count_not(0xF81F) == 0, "V23 240 rows of 0xF81F over a full window == reset(0xF81F)");
    check(panel::stats().transactions == 0 && panel::stats().pixel_bytes == 0, "V24 reset() clears the transaction statistics");

    // Backlight.
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    const uint32_t d512 = panel::backlight_duty();
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4000);
    const uint32_t d_clamped = panel::backlight_duty();
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    const uint32_t d0 = panel::backlight_duty();
    check(d512 == 512 && d_clamped == 1023 && d0 == 0, "V25 ledc_set_duty stores the backlight duty (512; 4000 clamps to 1023; 0)",
          format("%u %u %u", d512, d_clamped, d0));
}

openu5::FrontendView fixed_view(int selected) {
    openu5::FrontendView v{};
    v.state = openu5::FrontendState::MainMenu;
    v.kind = openu5::FrontendViewKind::Menu;
    v.title = "Ultima V Native";
    v.subtitle = "Windows test view";
    v.lines[0] = "Continue";
    v.lines[1] = "New Journey";
    v.lines[2] = "Load Journey";
    v.lines[3] = "Settings";
    v.lines[4] = "Credits";
    v.line_count = 5;
    v.selected_line = selected;
    v.footer = "Enter: choose   Esc: back";
    return v;
}

void real_board() {
    {
        auto cold = std::make_unique<tdeck::Board>();
        check(cold->show_frontend(fixed_view(1)) == ESP_ERR_INVALID_STATE,
              "B0 show_frontend before initialize_display: ESP_ERR_INVALID_STATE");
    }
    panel::reset(0x0000);
    auto board = std::make_unique<tdeck::Board>();
    board->set_release_label("Windows test");
    const esp_err_t init = board->initialize_display();
    check(init == ESP_OK, "B1 the real tdeck::Board::initialize_display() returns ESP_OK on the virtual panel",
          format("err=%d", int(init)));
    check(panel::stats().malformed == 0 && panel::stats().windows > 0 && panel::stats().transactions > 0,
          "B2 the Board's whole start-up stream is well-formed (no malformed transaction)",
          format("transactions=%llu windows=%llu malformed=%llu", (unsigned long long)panel::stats().transactions,
                 (unsigned long long)panel::stats().windows, (unsigned long long)panel::stats().malformed));
    check(panel::backlight_duty() == 80u * 1023u / 100u, "B3 the boot backlight is 80 % (duty 818 of 1023)",
          format("duty=%u", panel::backlight_duty()));
    const size_t boot_lit = count_not(0x0000);
    check(boot_lit > 500, "B4 the boot screen is on the panel (title text drawn)", format("%zu non-black pixels", boot_lit));
    uvtest::note("boot screen hash %s (%zu non-black pixels)", uvtest::hex64(panel_hash()).c_str(), boot_lit);
    {
        // The Board configured the bus for 640-byte transactions.
        gpio_set_level(tdeck::pins::kTftDataCommand, 1);
        std::vector<uint8_t> big(641, 0);
        spi_transaction_t tx{};
        tx.length = big.size() * 8;
        tx.tx_buffer = big.data();
        const uint64_t m = panel::stats().malformed;
        check(spi_device_transmit(g_handle, &tx) == ESP_ERR_INVALID_ARG && panel::stats().malformed == m + 1,
              "B5 after the Board's spi_bus_initialize, a 641-byte transaction is refused (max_transfer_sz 640)");
    }

    const openu5::FrontendView view = fixed_view(1);
    panel::reset(0x0000);
    const uint64_t blank = panel_hash();
    const uint64_t gen = panel::generation();
    const esp_err_t shown = board->show_frontend(view);
    const uint64_t first = panel_hash();
    const size_t lit = count_not(0x0000);
    check(shown == ESP_OK && panel::generation() != gen && first != blank && lit > 1000,
          "B6 show_frontend(fixed FrontendView) draws onto the reset panel",
          format("err=%d lit=%zu", int(shown), lit));
    check(panel::stats().malformed == 0, "B7 the frontend draw is well-formed (no malformed transaction)");
    uvtest::note("FRONTEND_PANEL_FNV1A64 %s (fixed FrontendView: MainMenu/Menu, 5 lines, line 1 selected, ui_size 1, no art; "
                 "%zu non-black pixels)",
                 uvtest::hex64(first).c_str(), lit);

    // Determinism: a fresh Board on a reset panel draws the identical image.
    {
        panel::reset(0x0000);
        auto second = std::make_unique<tdeck::Board>();
        second->set_release_label("Windows test");
        const bool ok = second->initialize_display() == ESP_OK;
        panel::reset(0x0000);
        const bool drawn = second->show_frontend(view) == ESP_OK;
        const uint64_t again = panel_hash();
        check(ok && drawn && again == first, "B8 two identical show_frontend calls into a reset panel give identical pixel hashes",
              uvtest::hex64(again) + " vs " + uvtest::hex64(first));
    }
    // The first board again: the panel now holds the second board's identical
    // image; the same view again must leave it unchanged.
    board->show_frontend(view);
    check(panel_hash() == first, "B9 repeating the same view on the same Board leaves the panel unchanged",
          uvtest::hex64(panel_hash()));
    // Incremental redraw: another selection changes the panel, and going back
    // reproduces exactly the full draw (the Board relies on the panel keeping
    // what it sent, which is what the virtual panel must do).
    board->show_frontend(fixed_view(3));
    const uint64_t moved = panel_hash();
    board->show_frontend(view);
    const uint64_t back = panel_hash();
    check(moved != first, "B10 selecting another line changes the panel");
    check(back == first, "B11 returning to the first selection reproduces the full draw pixel for pixel",
          uvtest::hex64(back) + " vs " + uvtest::hex64(first));
    check(panel::stats().malformed == 0, "B12 no malformed transaction across all frontend draws");
}

// --- Part 3: Original's DOS game screen (board_dos_layout.cpp) -------------------
// ULTIMA.EXE's frame, 0x63ac..0x64f7, as the web port's fiel/frame.ts tables
// give it (fills in the band's blue, the three corner glyphs, the white
// segments), written out here independently of the Board's code. Without a
// font the corners are the player's IBM.CH glyphs as measured from captures.
uint16_t rom_frame_pixel(int x, int y) {
    struct Rect { int x0, y0, x1, y1; };
    static const Rect fills[] = {{0x00, 0x00, 0x13f, 0x06}, {0x00, 0xb9, 0xbf, 0xbf}, {0x00, 0x00, 0x06, 0xbf},
                                 {0xb9, 0x00, 0xbf, 0xbf},  {0x139, 0x00, 0x13f, 0x57}, {0xc0, 0x50, 0x138, 0x57},
                                 {0xc0, 0x39, 0x138, 0x3f}};
    static const Rect segments[] = {{0x07, 0x07, 0x07, 0xb8},  {0x07, 0xb8, 0xb8, 0xb8},  {0xb8, 0x07, 0xb8, 0xb8},
                                    {0x07, 0x07, 0xb8, 0x07},  {0xbf, 0x57, 0xbf, 0xbf},  {0xbf, 0x57, 0x13f, 0x57},
                                    {0xbf, 0x07, 0x138, 0x07}, {0x138, 0x07, 0x138, 0x38}, {0xbf, 0x38, 0x138, 0x38},
                                    {0xbf, 0x07, 0xbf, 0x38},  {0xbf, 0x3f, 0x138, 0x3f}, {0x138, 0x3f, 0x138, 0x50},
                                    {0xbf, 0x50, 0x138, 0x50}, {0xbf, 0x3f, 0xbf, 0x50}};
    static const uint8_t corners[3][8] = {{0x07, 0x1f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff},
                                          {0xe0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0xff, 0xff},
                                          {0xff, 0xff, 0xff, 0x7f, 0x7f, 0x3f, 0x1f, 0x07}};
    static const int corner_cells[3][2] = {{0, 0}, {39, 0}, {0, 23}};
    auto in = [&](const Rect &r) { return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1; };
    uint16_t p = 0x0000;
    for (const auto &r : fills)
        if (in(r)) p = 0x0015;
    for (int c = 0; c < 3; ++c) {
        const int cx = corner_cells[c][0] * 8, cy = corner_cells[c][1] * 8;
        if (x >= cx && x < cx + 8 && y >= cy && y < cy + 8)
            p = (corners[c][y - cy] & (0x80 >> (x - cx))) ? 0x0015 : 0x0000;
    }
    for (const auto &r : segments)
        if (in(r)) p = 0xFFFF;
    return p;
}


// The real Board composes the 1988 game's 320 x 200 screen on the panel's rows
// 0..199: ULTIMA.EXE's frame, the map whole at (8,8), the sky and wind bands
// outside it, the right panel's boxes (the world's, or a list's DeviceDosPanel)
// and the 16 x 13 console; the bed and camp draws and an ending page use the
// same rows; leaving the layout gives the handheld's 320 x 240 screen back.
void dos_screen() {
    constexpr uint16_t kBand = 0x0015, kRule = 0xFFFF, kBlack = 0x0000, kMap = 0x07E0;
    panel::reset(0x1234);
    auto board = std::make_unique<tdeck::Board>();
    check(board->initialize_display() == ESP_OK, "D0 a fresh Board on the virtual panel");
    board->set_dos_layout(true);
    check(board->dos_layout() && board->picture_height() == 200, "D1 set_dos_layout(true): the picture is 200 rows",
          format("%d", board->picture_height()));

    openu5::UiTextBlock storage[32]{};
    openu5::UiSession ui{{storage, 32}, {}, {16, 21, 12}};
    ui.set_console_layout(true);
    ui.append(openu5::UiTextChannel::Message, "Hello from 1988");
    openu5::GameState game{};
    game.party.character_count = 1;
    game.party.party_size = 1;
    game.party.active_character = 0;
    std::snprintf(game.party.characters[0].name, sizeof(game.party.characters[0].name), "Avatar");
    game.party.characters[0].current_hp = 150;
    game.party.characters[0].status = 'G';
    game.party.characters[0].party_status = 0;
    game.food = 63;
    game.gold = 150;
    game.time.month = 4;
    game.time.day = 5;
    game.time.year = 139;
    openu5::TurnState turn{};
    openu5::HudWorldState hud{};
    hud.sky_visible = hud.wind_visible = true;
    hud.wind = "West  Winds";
    hud.marks[0] = {int8_t(5), uint8_t(0x30), false};
    hud.mark_count = 1;
    std::vector<uint8_t> runes(128 * 8, 0);
    for (int r = 0; r < 8; ++r) runes[0x30 * 8 + r] = 0xFF; // a moon that fills its cell
    std::vector<uint16_t> map(176 * 176, kMap);
    auto draw = [&](const tdeck::DeviceSelectionView *selection = nullptr) {
        return board->show_alpha(map.data(), ui, game, turn, hud, runes.data(), nullptr, nullptr, false, nullptr, false, 1,
                                 nullptr, selection, nullptr, {}, 1);
    };
    check(draw() == ESP_OK && panel::stats().malformed == 0, "D2 show_alpha draws the DOS screen (well-formed stream)");
    struct Probe {
        int x, y;
        uint16_t want;
        const char *what;
    };
    auto probe = [&](const char *id, std::initializer_list<Probe> probes) {
        std::string bad;
        for (const auto &q : probes)
            if (at(q.x, q.y) != q.want && bad.empty())
                bad = format("%s at %d,%d is %04x, expected %04x", q.what, q.x, q.y, unsigned(at(q.x, q.y)), unsigned(q.want));
        check(bad.empty(), id, bad);
    };
    {
        // Every frame pixel outside what changes (the map, the two band
        // notches, the panel's window rows, the console) against the ROM tables.
        auto content = [](int x, int y) {
            return (x >= 8 && x <= 183 && y >= 8 && y <= 183) || (x >= 40 && x <= 151 && y <= 7) ||
                   (x >= 48 && x <= 151 && y >= 184 && y <= 191) || (x >= 192 && x <= 311 && y >= 8 && y <= 55) ||
                   (x >= 192 && x <= 311 && y >= 64 && y <= 79) || (x >= 192 && y >= 88 && y <= 191);
        };
        std::string bad;
        size_t compared = 0;
        for (int y = 0; y < 200; ++y)
            for (int x = 0; x < panel::kWidth; ++x) {
                if (content(x, y)) continue;
                ++compared;
                if (at(x, y) != rom_frame_pixel(x, y) && bad.empty())
                    bad = format("%d,%d is %04x, ULTIMA.EXE's frame %04x", x, y, unsigned(at(x, y)), unsigned(rom_frame_pixel(x, y)));
            }
        check(bad.empty(), format("D3a the frame is ULTIMA.EXE's (0x63ac..0x64f7) at all %zu pixels outside the content", compared),
              bad);
    }
    probe("D3 the frame: the map's box rules at x 7 / 184, the bands, the panel's and console's rules, the cut corners",
          {{7, 100, kRule, "map box left rule"},
           {184, 100, kRule, "map box right rule"},
           {20, 7, kRule, "map box top rule"},
           {20, 184, kRule, "map box bottom rule"},
           {3, 100, kBand, "left band"},
           {188, 100, kBand, "separator band"},
           {191, 30, kRule, "roster box left rule"},
           {312, 30, kRule, "roster box right rule"},
           {316, 40, kBand, "right strip"},
           {200, 56, kRule, "roster box bottom rule"},
           {200, 60, kBand, "bar between the boxes"},
           {200, 63, kRule, "status box top rule"},
           {200, 80, kRule, "status box bottom rule"},
           {200, 84, kBand, "band over the console"},
           {300, 87, kRule, "console top rule"},
           {191, 140, kRule, "console left rule"},
           {0, 0, kBlack, "top-left corner"},
           {319, 0, kBlack, "top-right corner"},
           {0, 191, kBlack, "bottom-left corner"},
           {100, 195, kBlack, "row 24, left"},
           {300, 195, kBlack, "row 24, under the console"}});
    size_t below = 0;
    for (int y = 200; y < panel::kHeight; ++y)
        for (int x = 0; x < panel::kWidth; ++x) below += at(x, y) != kBlack;
    check(below == 0, "D4 the panel's rows 200..239 are black (the presenter never shows them)", format("%zu lit", below));
    probe("D5 the map is whole at (8,8)-(183,183): no strip covers its top or bottom rows",
          {{8, 8, kMap, "map top-left"}, {183, 8, kMap, "map top-right"}, {8, 183, kMap, "map bottom-left"},
           {183, 183, kMap, "map bottom-right"}, {100, 10, kMap, "map row 2"}, {100, 180, kMap, "map row 172"}});
    probe("D6 the sky band: > at x 40, the notch x 48..143 black, a moon in cell 5 (x 88..95)",
          {{40, 0, kRule, "> edge"}, {40, 7, kRule, "> edge on the rule row"}, {44, 2, kRule, "> edge"},
           {50, 3, kBlack, "notch"}, {90, 3, kRule, "moon"}, {151, 0, kRule, "< edge"}, {30, 3, kBand, "band left of >"}});
    probe("D7 the wind band: >West  Winds< from col 6 (> at x 48, < at x 144)",
          {{48, 184, kRule, "> edge (the rule's end)"}, {49, 185, kRule, "> edge"}, {54, 188, kBlack, "> notch"},
           {151, 191, kRule, "< edge"}, {40, 188, kBand, "band left of >"}, {160, 188, kBand, "band right of <"}});
    auto lit = [&](int x0, int y0, int x1, int y1) {
        size_t n = 0;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) n += at(x, y) == kRule;
        return n;
    };
    check(lit(192, 8, 311, 15) > 20 && lit(192, 16, 311, 55) == 0 && lit(192, 64, 311, 71) > 20 && lit(192, 72, 311, 79) > 10,
          "D8 the panel: one roster row, F/G and the date in their window rows",
          format("roster %zu, rows 2-6 %zu, F/G %zu, date %zu", lit(192, 8, 311, 15), lit(192, 16, 311, 55),
                 lit(192, 64, 311, 71), lit(192, 72, 311, 79)));
    // panel.ts: "G:150" right-aligned to cell 14 (x 272..311), "4-5-139" centred
    // in the 15 cells (cells 4..10, x 224..279), nothing in cells 0..3; the
    // roster's HP right-aligned in cells 10..13 and the status letter in 14.
    check(lit(304, 64, 311, 71) > 3 && lit(232, 64, 271, 71) == 0 && lit(192, 72, 223, 79) == 0 &&
              lit(224, 72, 231, 79) > 3 && lit(272, 72, 279, 79) > 3 && lit(280, 72, 311, 79) == 0 &&
              lit(304, 8, 311, 15) > 3,
          "D8b the status box: G right-aligned, the date 4-5-139 centred (cells 4..10); the status letter in cell 14",
          format("G cell 14 %zu, F/G gap %zu, date cells 0-3 %zu / 4 %zu / 10 %zu / 11-14 %zu, roster cell 14 %zu",
                 lit(304, 64, 311, 71), lit(232, 64, 271, 71), lit(192, 72, 223, 79), lit(224, 72, 231, 79),
                 lit(272, 72, 279, 79), lit(280, 72, 311, 79), lit(304, 8, 311, 15)));
    check(lit(192, 184, 319, 191) > 30 && lit(192, 88, 319, 183) == 0,
          "D9 the console: its one line on the bottom row (y 184..191), the 12 rows above it empty",
          format("bottom %zu, above %zu", lit(192, 184, 319, 191), lit(192, 88, 319, 183)));
    {
        // The console wraps where the screen ends: 16 cells. "The quick brown
        // fox jumps" is "The quick brown" / "fox jumps" there; a 22-cell
        // wrap would leave "jumps" alone on the bottom row (cells 0..4).
        openu5::UiTextBlock wrap_storage[8]{};
        openu5::UiSession wrap{{wrap_storage, 8}, {}, {16, 21, 12}};
        wrap.set_console_layout(true);
        wrap.append(openu5::UiTextChannel::Message, "The quick brown fox jumps");
        check(board->show_alpha(map.data(), wrap, game, turn, hud, runes.data(), nullptr, nullptr, false, nullptr, false, 1,
                                nullptr, nullptr, nullptr, {}, 1) == ESP_OK &&
                  lit(192 + 6 * 8, 184, 192 + 9 * 8 - 1, 191) > 10 && lit(192 + 13 * 8, 176, 192 + 15 * 8 - 1, 183) > 5,
              "D9b the console wraps at 16 cells: \"fox jumps\" fills cells 0..8 of the bottom row",
              format("bottom cells 6..8: %zu, row above cells 13..14: %zu", lit(240, 184, 263, 191), lit(296, 176, 311, 183)));
    }
    draw();
    const uint64_t world = panel_hash();
    draw();
    check(panel_hash() == world, "D10 the same frame again changes nothing");

    // A list: the 1988 right panel as one box (banner, bar, arrow).
    tdeck::DeviceDosPanel list{};
    tdeck::dos_panel_clear(list);
    std::snprintf(list.banner, sizeof(list.banner), "Items:");
    tdeck::dos_panel_list_frame(list, 7);
    tdeck::dos_panel_put(list, 1, 1, "First");
    list.inverse[1] = 0x3ffe;
    list.arrow = tdeck::kDosArrowDown;
    tdeck::DeviceSelectionView view{};
    view.active = true;
    view.dos = &list;
    check(draw(&view) == ESP_OK, "D11 a DOS list panel draws");
    probe("D12 the list: one box (its sides joined across y 57..62), the banner's > at x 216, the arrow's notch at x 240",
          {{191, 59, kRule, "box left side across the bar"},
           {312, 59, kRule, "box right side across the bar"},
           {216, 0, kRule, "banner > edge"},
           {218, 3, kBand, "banner > fill"},
           {240, 80, kRule, "arrow > edge (the rule's end)"},
           {242, 80, kBlack, "arrow notch over the rule"},
           {263, 87, kRule, "arrow < edge on the console's rule"},
           {200, 56, kBlack, "no roster rule inside the box"},
           {200, 60, kBlack, "no bar inside the box"}});
    check(lit(200, 16, 303, 23) > 200, "D13 the bar row is in reverse video (cols 1..13)", format("%zu", lit(200, 16, 303, 23)));
    check(draw() == ESP_OK && panel_hash() == world, "D14 closing the list gives the world screen back pixel for pixel");

    // The bed and the camp: the whole 176 x 176 game window, as the original.
    check(board->fill_bed_viewport() == ESP_OK && at(8, 8) == kBlack && at(183, 183) == kBlack && at(7, 100) == kRule,
          "D15 fill_bed_viewport blacks the whole map window (8,8)-(183,183), the frame stays");
    std::vector<uint16_t> camp(176 * 176, 0xF800);
    check(board->show_camp_viewport(camp.data(), 7) == ESP_OK && at(8, 8) == 0xF800 && at(183, 183) == 0xF800 &&
              at(20, 7) == kRule,
          "D16 show_camp_viewport draws the 176 x 176 scene at (8,8)");
    check(board->refresh_bed_status_panel(game, {}) == ESP_OK && lit(192, 8, 311, 15) > 20,
          "D17 refresh_bed_status_panel redraws the roster in the DOS panel");
    check(draw() == ESP_OK && panel_hash() == world, "D18 the next frame restores the map");

    // The frontend is the handheld's 320 x 240 screen; the DOS screen comes back whole.
    check(board->show_frontend(fixed_view(1)) == ESP_OK && board->picture_height() == 240,
          "D19 the frontend fills the panel: picture height 240");
    check(draw() == ESP_OK && board->picture_height() == 200 && panel_hash() == world,
          "D20 back in play the DOS screen is redrawn whole, pixel for pixel");

    // An ending page (ENDGAME.OVL's 320 x 200 picture) on the same rows.
    std::vector<uint8_t> page(320 * 200 / 2, 0x11);
    uint16_t palette[16]{};
    palette[1] = 0x001F;
    check(board->show_endgame_page(page.data(), palette, nullptr, nullptr, nullptr, 1, true) == ESP_OK &&
              at(0, 0) == 0x001F && at(319, 199) == 0x001F && at(0, 200) == kBlack && board->picture_height() == 200,
          "D21 an ending page is the panel's rows 0..199 on the DOS screen (the handheld's starts at row 20)");

    // Leaving the layout: the handheld's screen, whole.
    board->set_dos_layout(false);
    check(draw() == ESP_OK && board->picture_height() == 240 && at(180, 100) == kRule && at(8, 100) == kMap,
          "D22 set_dos_layout(false): the handheld's 320 x 240 screen (its play window's rule at x 180)",
          format("height %d, (180,100) %04x", board->picture_height(), unsigned(at(180, 100))));
    board->set_dos_layout(true);
    check(draw() == ESP_OK && panel_hash() == world, "D23 and back: the DOS screen pixel for pixel");
    check(panel::stats().malformed == 0, "D24 no malformed transaction across the DOS screen's draws");
}

} // namespace

int main() {
    std::printf("uv_virtual_panel_test: the virtual ST7789 driven by hand, then by the real tdeck::Board\n");
    driver_level();
    real_board();
    dos_screen();
    return uvtest::finish("uv_virtual_panel_test");
}
