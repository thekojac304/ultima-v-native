#pragma once
// Ultima V Native for Windows: the FreeRTOS task calls the shared runtime and
// Board make on the game thread. vTaskDelay(n) sleeps n ticks of 10 ms (the
// device's CONFIG_FREERTOS_HZ=100); vTaskDelay(0) and taskYIELD() give up the
// rest of the time slice. The stack high-water mark reports the main thread's
// remaining stack, which the runtime only logs.
#include "FreeRTOS.h"

using TaskHandle_t = void *;
#ifndef CONFIG_ESP_MAIN_TASK_STACK_SIZE
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE (1024 * 1024)
#endif

void vTaskDelay(TickType_t ticks);
void uv_task_yield();
#define taskYIELD() uv_task_yield()
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task);
