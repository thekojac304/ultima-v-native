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

} // namespace

int main() {
    std::printf("uv_virtual_panel_test: the virtual ST7789 driven by hand, then by the real tdeck::Board\n");
    driver_level();
    real_board();
    return uvtest::finish("uv_virtual_panel_test");
}
