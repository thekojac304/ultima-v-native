#pragma once
// Ultima V Native for Windows: the ESP-IDF error vocabulary the shared T-Deck
// application layer (native/targets/tdeck/main) is written against. Values are
// ESP-IDF's own (components/esp_common/include/esp_err.h), so every log line
// and every comparison means on Windows exactly what it means on the device.
//
// esp_compat/ is the Windows platform's production implementation of the small
// ESP-IDF surface the shared runtime touches. It is deliberately separate from
// native/targets/tdeck/host_tests/{esp,board,sd}_shims, which are test seams
// (virtual clocks, fault injection, timing models) and must never ship.
#include <cstdint>

typedef int32_t esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#define ESP_ERR_INVALID_MAC 0x10B
#define ESP_ERR_NOT_FINISHED 0x10C

inline const char *esp_err_to_name(esp_err_t code) {
    switch (code) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_RESPONSE: return "ESP_ERR_INVALID_RESPONSE";
    case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC";
    case ESP_ERR_INVALID_VERSION: return "ESP_ERR_INVALID_VERSION";
    case ESP_ERR_NOT_FINISHED: return "ESP_ERR_NOT_FINISHED";
    default: return "ESP_ERR_UNKNOWN";
    }
}
