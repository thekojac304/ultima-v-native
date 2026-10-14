#pragma once
// Ultima V Native for Windows: ESP-IDF memory-placement attributes. They pick
// a RAM/flash segment on the ESP32-S3 and have no meaning on a desktop.
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_NOINIT_ATTR
#define RTC_DATA_ATTR
#define EXT_RAM_ATTR
#define EXT_RAM_BSS_ATTR
#define IRAM_ATTR_STARTUP
