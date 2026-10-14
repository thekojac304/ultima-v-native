#pragma once
// Ultima V Native for Windows: ESP-IDF's FAT-on-SD mount API. On the T-Deck,
// esp_vfs_fat_sdspi_mount("/sd") makes the card's files reachable as "/sd/...".
// On Windows the "/sd" prefix is mapped to the data directory by the storage
// layer (src/platform/storage.cpp, force-included as uv_storage_redirect.h),
// and this mount call only checks that the directory is usable.
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "driver/sdspi_host.h"
#include "esp_err.h"

typedef struct {
    struct {
        uint32_t capacity;
        uint32_t sector_size;
    } csd;
    bool is_mmc;
    uint32_t ocr;
    int real_freq_khz;
} sdmmc_card_t;
#define SD_OCR_SDHC_CAP (1u << 30)

typedef struct {
    bool format_if_mount_failed;
    int max_files;
    size_t allocation_unit_size;
    bool disk_status_check_enable;
    bool use_one_fat;
} esp_vfs_fat_sdmmc_mount_config_t;

esp_err_t esp_vfs_fat_sdspi_mount(const char *base_path, const sdmmc_host_t *host,
                                  const sdspi_device_config_t *slot, const esp_vfs_fat_sdmmc_mount_config_t *config,
                                  sdmmc_card_t **card);
esp_err_t esp_vfs_fat_sdcard_unmount(const char *base_path, sdmmc_card_t *card);
void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card);
/** Total and free bytes of the volume holding the data directory (GetDiskFreeSpaceExW). */
esp_err_t esp_vfs_fat_info(const char *base_path, uint64_t *total_bytes, uint64_t *free_bytes);
