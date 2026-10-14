#pragma once
// Ultima V Native for Windows: ESP-IDF's esp_check.h. Same control flow as
// the real macro: on failure, log the message and return the error at once.
#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR(x, tag, msg, ...)                                                  \
    do {                                                                                       \
        const esp_err_t uv_check_err_ = (x);                                                   \
        if (uv_check_err_ != ESP_OK) {                                                         \
            ESP_LOGE(tag, "%s(%d): " msg, __func__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);       \
            return uv_check_err_;                                                              \
        }                                                                                      \
    } while (0)
