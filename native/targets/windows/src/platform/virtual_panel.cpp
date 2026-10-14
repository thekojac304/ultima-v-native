#include "platform/virtual_panel.h"

#include <array>
#include <atomic>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "tdeck_pins.h"

struct uv_spi_device {
    int id = 0;
};

namespace uv::panel {
namespace {
std::array<uint16_t, kWidth * kHeight> g_gram{};
std::atomic<uint64_t> g_generation{0};
std::atomic<uint32_t> g_duty{1023};
Stats g_stats{};
int g_dc = 0; // the TFT's D/C line: 0 = command, 1 = data
uint8_t g_command = 0;
uint8_t g_params[4]{};
int g_param_count = 0;
int g_x0 = 0, g_x1 = kWidth - 1, g_y0 = 0, g_y1 = kHeight - 1, g_x = 0, g_y = 0;
uint32_t g_max_transfer = 4092; // ESP-IDF's DMA default; the Board configures 640
uv_spi_device g_display{1};

void write_pixel(uint16_t value) {
    if (g_x < 0 || g_x >= kWidth || g_y < 0 || g_y >= kHeight) {
        ++g_stats.malformed;
    } else {
        g_gram[size_t(g_y) * kWidth + size_t(g_x)] = value;
    }
    if (++g_x > g_x1) {
        g_x = g_x0;
        if (++g_y > g_y1) g_y = g_y0;
    }
}

// The ST7789's side of one transaction: a command byte, its parameters, or pixels.
void decode(const uint8_t *data, size_t bytes) {
    if (g_dc == 0) {
        if (bytes != 1) ++g_stats.malformed;
        g_command = bytes ? data[0] : 0;
        g_param_count = 0;
        if (g_command == 0x2C) { // RAMWR: the write pointer returns to the window's start
            g_x = g_x0;
            g_y = g_y0;
            ++g_stats.windows;
        }
        return;
    }
    if (g_command == 0x2A || g_command == 0x2B) { // CASET / RASET: start and end, big-endian
        for (size_t i = 0; i < bytes && g_param_count < 4; ++i) g_params[g_param_count++] = data[i];
        if (g_param_count == 4) {
            const int start = g_params[0] << 8 | g_params[1];
            const int end = g_params[2] << 8 | g_params[3];
            if (g_command == 0x2A) {
                g_x0 = start;
                g_x1 = end;
            } else {
                g_y0 = start;
                g_y1 = end;
            }
        }
        return;
    }
    if (g_command == 0x2C) {
        if (bytes % 2) ++g_stats.malformed; // RGB565: whole pixels only
        g_stats.pixel_bytes += bytes;
        for (size_t i = 0; i + 1 < bytes; i += 2) write_pixel(uint16_t(data[i] << 8 | data[i + 1]));
        g_generation.fetch_add(1, std::memory_order_release);
    }
}
} // namespace

const uint16_t *pixels() { return g_gram.data(); }
uint64_t generation() { return g_generation.load(std::memory_order_acquire); }
uint32_t backlight_duty() { return g_duty.load(); }
const Stats &stats() { return g_stats; }

void reset(uint16_t fill) {
    g_gram.fill(fill);
    g_stats = Stats{};
    g_dc = 0;
    g_command = 0;
    g_param_count = 0;
    g_x0 = g_x = 0;
    g_y0 = g_y = 0;
    g_x1 = kWidth - 1;
    g_y1 = kHeight - 1;
    g_generation.fetch_add(1, std::memory_order_release);
}

} // namespace uv::panel

using namespace uv::panel;

esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
esp_err_t gpio_set_level(gpio_num_t gpio, uint32_t level) {
    if (gpio == tdeck::pins::kTftDataCommand) g_dc = level ? 1 : 0;
    return ESP_OK;
}
esp_err_t gpio_output_enable(gpio_num_t) { return ESP_OK; }
esp_err_t gpio_set_pull_mode(gpio_num_t, gpio_pull_mode_t) { return ESP_OK; }

esp_err_t ledc_timer_config(const ledc_timer_config_t *) { return ESP_OK; }
esp_err_t ledc_channel_config(const ledc_channel_config_t *) { return ESP_OK; }
esp_err_t ledc_set_duty(ledc_mode_t, ledc_channel_t, uint32_t duty) {
    g_duty.store(duty > 1023 ? 1023u : duty);
    return ESP_OK;
}
esp_err_t ledc_update_duty(ledc_mode_t, ledc_channel_t) { return ESP_OK; }

esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t *config, spi_dma_chan_t) {
    g_max_transfer = config && config->max_transfer_sz > 0 ? uint32_t(config->max_transfer_sz) : 4092u;
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t *, spi_device_handle_t *handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    *handle = &g_display;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *t) {
    if (!handle || !t) return ESP_ERR_INVALID_ARG;
    // The driver refuses a transaction longer than the bus's max_transfer_sz
    // (ESP-IDF spi_master.c check_trans_valid); so does the virtual bus, so a
    // Board change that would fail on the device fails here too.
    if (t->length > size_t(g_max_transfer) * 8) {
        ++g_stats.malformed;
        return ESP_ERR_INVALID_ARG;
    }
    const bool txdata = (t->flags & SPI_TRANS_USE_TXDATA) != 0;
    const size_t bytes = t->length / 8;
    const uint8_t *data = txdata ? t->tx_data : static_cast<const uint8_t *>(t->tx_buffer);
    if (t->length % 8 || (bytes && !data)) ++g_stats.malformed;
    ++g_stats.transactions;
    if (data) decode(data, bytes);
    return ESP_OK;
}
