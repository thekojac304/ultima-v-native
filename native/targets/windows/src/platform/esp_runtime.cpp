// Ultima V Native for Windows: the clock, task and heap calls of the ESP-IDF
// compatibility layer (esp_compat/). See esp_compat/esp_err.h for the scope.
#include <chrono>
#include <cstdint>
#include <thread>

#include <windows.h>

#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace {
using Clock = std::chrono::steady_clock;
// The process's own epoch: the runtime truncates the clock to uint32
// milliseconds in many places, so it counts from start-up, as the device's
// esp_timer counts from boot (49.7 days before a wrap).
const Clock::time_point g_start = Clock::now();

size_t available_memory() {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return size_t(256) * 1024 * 1024;
    const uint64_t avail = status.ullAvailPhys < status.ullAvailVirtual ? status.ullAvailPhys : status.ullAvailVirtual;
    // The runtime prints these and compares them against a few-KiB reserve;
    // a cap keeps the figures readable and inside size_t on any build.
    const uint64_t cap = uint64_t(1) << 31;
    return size_t(avail < cap ? avail : cap);
}
} // namespace

int64_t esp_timer_get_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - g_start).count();
}

esp_cpu_cycle_count_t esp_cpu_get_cycle_count() {
    const uint64_t us = uint64_t(esp_timer_get_time());
    return esp_cpu_cycle_count_t(us * uint64_t(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ));
}

void vTaskDelay(TickType_t ticks) {
    if (ticks == 0) {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(ticks) * portTICK_PERIOD_MS));
}

void uv_task_yield() { std::this_thread::yield(); }

unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) {
    // The game thread's remaining stack, as the device reports its task's.
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    volatile char marker = 0;
    const ULONG_PTR here = reinterpret_cast<ULONG_PTR>(&marker);
    return here > low ? unsigned(here - low) : 0u;
}

size_t heap_caps_get_free_size(uint32_t) { return available_memory(); }
size_t heap_caps_get_largest_free_block(uint32_t) { return available_memory(); }
size_t heap_caps_get_minimum_free_size(uint32_t) { return available_memory(); }
size_t heap_caps_get_total_size(uint32_t) { return available_memory(); }
