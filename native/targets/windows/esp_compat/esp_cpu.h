#pragma once
// Ultima V Native for Windows: the CPU cycle counter the Board uses to time
// its panel transactions for the Developer performance report. It counts at
// CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ from the steady clock, so the report's
// microsecond figures stay meaningful (they then describe the virtual panel).
#include <cstdint>
typedef uint32_t esp_cpu_cycle_count_t;
esp_cpu_cycle_count_t esp_cpu_get_cycle_count();
