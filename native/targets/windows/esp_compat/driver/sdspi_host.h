#pragma once
// Ultima V Native for Windows: ESP-IDF's driver/sdspi_host.h, so the shared
// Board's SD-card code compiles. The Windows "card" is the data directory
// (src/platform/storage.cpp); see esp_vfs_fat.h.
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

typedef struct {
    int slot;
    int max_freq_khz;
} sdmmc_host_t;
#define SDSPI_HOST_DEFAULT() sdmmc_host_t{1, 20000}
#define SDSPI_SLOT_NO_CD GPIO_NUM_NC
#define SDSPI_SLOT_NO_WP GPIO_NUM_NC
#define SDSPI_SLOT_NO_INT GPIO_NUM_NC
#define SDSPI_IO_ACTIVE_LOW 0

typedef struct {
    spi_host_device_t host_id;
    gpio_num_t gpio_cs;
    gpio_num_t gpio_cd;
    gpio_num_t gpio_wp;
    gpio_num_t gpio_int;
    bool gpio_wp_polarity;
    uint16_t duty_cycle_pos;
    uint8_t wait_for_miso;
} sdspi_device_config_t;
