#pragma once
// Ultima V Native for Windows: the FreeRTOS types the shared runtime names.
// The Windows build runs the game on one thread (as the device does: the game
// loop is a single task); only the audio callback runs elsewhere, and it uses
// std:: primitives, not these.
#include <cstdint>

#include "sdkconfig.h"

using TickType_t = uint32_t;
using StackType_t = uint8_t;
using BaseType_t = int;
using UBaseType_t = unsigned;
#define portTICK_PERIOD_MS (1000 / CONFIG_FREERTOS_HZ)
#define pdMS_TO_TICKS(ms) ((TickType_t)(((uint64_t)(ms) * CONFIG_FREERTOS_HZ) / 1000))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
