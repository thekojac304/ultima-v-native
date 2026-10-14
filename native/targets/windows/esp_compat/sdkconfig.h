#pragma once
// Ultima V Native for Windows: the two sdkconfig values the shared Board code
// reads. CONFIG_FREERTOS_HZ is the tick the runtime's loop_wait_ticks() counts
// in (one tick = 10 ms), which the Windows loop honours as a wait timeout.
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ 240
#define CONFIG_FREERTOS_HZ 100
