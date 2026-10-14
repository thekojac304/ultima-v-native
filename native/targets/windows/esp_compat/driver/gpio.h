#pragma once
// Ultima V Native for Windows: ESP-IDF's driver/gpio.h, as far as the shared
// Board (native/targets/tdeck/main/tdeck_board.cpp) uses it. The one level
// that matters is the panel's D/C line (pins::kTftDataCommand), which the
// virtual panel (src/platform/virtual_panel.cpp) reads to tell a command byte
// from data -- exactly what the ST7789 does with the physical pin.
#include <cstdint>

#include "esp_err.h"

typedef enum {
    GPIO_NUM_NC = -1,
    GPIO_NUM_0 = 0, GPIO_NUM_1 = 1, GPIO_NUM_2 = 2, GPIO_NUM_3 = 3, GPIO_NUM_4 = 4, GPIO_NUM_5 = 5,
    GPIO_NUM_6 = 6, GPIO_NUM_7 = 7, GPIO_NUM_8 = 8, GPIO_NUM_9 = 9, GPIO_NUM_10 = 10, GPIO_NUM_11 = 11,
    GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15, GPIO_NUM_16 = 16,
    GPIO_NUM_17 = 17, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19, GPIO_NUM_20 = 20, GPIO_NUM_21 = 21,
    GPIO_NUM_38 = 38, GPIO_NUM_39 = 39, GPIO_NUM_40 = 40, GPIO_NUM_41 = 41, GPIO_NUM_42 = 42,
    GPIO_NUM_43 = 43, GPIO_NUM_44 = 44, GPIO_NUM_45 = 45, GPIO_NUM_46 = 46, GPIO_NUM_47 = 47,
    GPIO_NUM_48 = 48,
} gpio_num_t;

typedef enum { GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT, GPIO_MODE_OUTPUT } gpio_mode_t;
typedef enum { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE } gpio_pulldown_t;
typedef enum { GPIO_INTR_DISABLE = 0, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE } gpio_int_type_t;
typedef enum { GPIO_PULLUP_ONLY = 0, GPIO_PULLDOWN_ONLY, GPIO_PULLUP_PULLDOWN, GPIO_FLOATING } gpio_pull_mode_t;

// Field order follows ESP-IDF 6.1 (C++20 designated initialisers need it).
typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(gpio_num_t gpio, uint32_t level);
esp_err_t gpio_output_enable(gpio_num_t gpio);
esp_err_t gpio_set_pull_mode(gpio_num_t gpio, gpio_pull_mode_t pull);
