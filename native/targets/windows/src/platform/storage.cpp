#include "platform/storage.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <chrono>

#include <io.h>
#include <windows.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"

namespace uv::storage {
namespace {
std::wstring g_root;
constexpr char kMount[] = "/sd";

bool is_mount_path(const char *path) {
    return path && std::strncmp(path, kMount, 3) == 0 && (path[3] == 0 || path[3] == '/');
}
} // namespace

std::wstring utf8_to_wide(const std::string &text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n);
    return out;
}

std::string wide_to_utf8(const std::wstring &text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}

void set_root(const std::wstring &directory) {
    g_root = directory;
    while (!g_root.empty() && (g_root.back() == L'\\' || g_root.back() == L'/')) g_root.pop_back();
}

const std::wstring &root() { return g_root; }

std::wstring map_path(const char *path) {
    if (!path) return {};
    std::wstring out;
    if (is_mount_path(path)) {
        out = g_root + utf8_to_wide(path + 3);
    } else {
        out = utf8_to_wide(path);
    }
    for (auto &c : out)
        if (c == L'/') c = L'\\';
    return out;
}

bool ensure_directory(const std::wstring &directory) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(directory), ec);
    return std::filesystem::is_directory(std::filesystem::path(directory), ec);
}

} // namespace uv::storage

using uv::storage::map_path;

namespace {
int errno_from_win32(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return ENOENT;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return EEXIST;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION: return EACCES;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL: return ENOSPC;
    default: return EIO;
    }
}

bool transient(DWORD error) {
    // Antivirus scanners and the search indexer briefly open a freshly
    // written file; a short retry rides that out instead of failing a save.
    return error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION || error == ERROR_ACCESS_DENIED;
}
} // namespace

std::FILE *uv_fopen(const char *path, const char *mode) {
    if (!path || !mode) {
        errno = EINVAL;
        return nullptr;
    }
    const std::wstring wide_path = map_path(path);
    const std::wstring wide_mode = uv::storage::utf8_to_wide(mode);
    return _wfopen(wide_path.c_str(), wide_mode.c_str());
}

int uv_rename(const char *from, const char *to) {
    // FATFS semantics, as the device code expects (it unlinks first): the
    // rename fails when the destination exists. WRITE_THROUGH returns only
    // once the move is on disk.
    const std::wstring a = map_path(from), b = map_path(to);
    for (int attempt = 0;; ++attempt) {
        if (MoveFileExW(a.c_str(), b.c_str(), MOVEFILE_WRITE_THROUGH)) return 0;
        const DWORD error = GetLastError();
        if (attempt < 10 && transient(error)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        errno = errno_from_win32(error);
        return -1;
    }
}

int uv_unlink(const char *path) {
    const std::wstring p = map_path(path);
    for (int attempt = 0;; ++attempt) {
        if (DeleteFileW(p.c_str())) return 0;
        const DWORD error = GetLastError();
        if (attempt < 10 && transient(error) && error != ERROR_ACCESS_DENIED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        errno = errno_from_win32(error);
        return -1;
    }
}

int uv_mkdir(const char *path, int) {
    const std::wstring p = map_path(path);
    if (CreateDirectoryW(p.c_str(), nullptr)) return 0;
    errno = errno_from_win32(GetLastError());
    return -1;
}

int uv_stat(const char *path, struct stat *out) {
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    const std::wstring p = map_path(path);
    struct _stat64 st {};
    if (_wstat64(p.c_str(), &st) != 0) return -1;
    std::memset(out, 0, sizeof(*out));
    out->st_mode = static_cast<decltype(out->st_mode)>(st.st_mode);
    out->st_size = static_cast<decltype(out->st_size)>(st.st_size);
    out->st_mtime = static_cast<decltype(out->st_mtime)>(st.st_mtime);
    out->st_atime = static_cast<decltype(out->st_atime)>(st.st_atime);
    out->st_ctime = static_cast<decltype(out->st_ctime)>(st.st_ctime);
    out->st_nlink = 1;
    return 0;
}

int uv_fsync(int fd) {
    // The A4-SAVE1 recovery model relies on a generation being on disk before
    // its commit record is written; _commit is FlushFileBuffers on the handle.
    return _commit(fd);
}

int uv_fileno(std::FILE *file) { return _fileno(file); }

// --- ESP-IDF FAT mount -----------------------------------------------------

namespace {
sdmmc_card_t g_card{};
}

esp_err_t esp_vfs_fat_sdspi_mount(const char *base_path, const sdmmc_host_t *host, const sdspi_device_config_t *,
                                  const esp_vfs_fat_sdmmc_mount_config_t *, sdmmc_card_t **card) {
    if (!base_path || std::strcmp(base_path, "/sd") != 0 || uv::storage::root().empty()) return ESP_FAIL;
    if (!uv::storage::ensure_directory(uv::storage::root())) return ESP_FAIL;
    uint64_t total = 0, free = 0;
    esp_vfs_fat_info(base_path, &total, &free);
    g_card = sdmmc_card_t{};
    g_card.csd.sector_size = 512;
    g_card.csd.capacity = uint32_t(std::min<uint64_t>(total / 512, 0xffffffffull));
    g_card.ocr = SD_OCR_SDHC_CAP;
    g_card.real_freq_khz = host ? host->max_freq_khz : 0;
    if (card) *card = &g_card;
    return ESP_OK;
}

esp_err_t esp_vfs_fat_sdcard_unmount(const char *, sdmmc_card_t *) { return ESP_OK; }

void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card) {
    if (!stream || !card) return;
    std::fprintf(stream, "Data directory: %s\n", uv::storage::wide_to_utf8(uv::storage::root()).c_str());
}

esp_err_t esp_vfs_fat_info(const char *, uint64_t *total_bytes, uint64_t *free_bytes) {
    ULARGE_INTEGER available{}, total{}, free{};
    const std::wstring &dir = uv::storage::root();
    if (dir.empty() || !GetDiskFreeSpaceExW(dir.c_str(), &available, &total, &free)) {
        ESP_LOGW("Storage", "free-space query failed for the data directory (error %lu)", (unsigned long)GetLastError());
        return ESP_FAIL;
    }
    if (total_bytes) *total_bytes = total.QuadPart;
    if (free_bytes) *free_bytes = available.QuadPart; // what this user may write
    return ESP_OK;
}
