#include "platform/log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>

#include "esp_log.h"
#include "esp_timer.h"

namespace uv::log {
namespace {
std::mutex g_mutex;
std::atomic<int> g_level{ESP_LOG_INFO};
std::FILE *g_file = nullptr;
std::wstring g_path;
size_t g_bytes = 0;
bool g_console = false;
std::atomic<size_t> g_lines{0};

char level_letter(int level) {
    switch (level) {
    case ESP_LOG_ERROR: return 'E';
    case ESP_LOG_WARN: return 'W';
    case ESP_LOG_INFO: return 'I';
    case ESP_LOG_DEBUG: return 'D';
    default: return 'V';
    }
}

void rotate_locked() {
    if (!g_file || g_bytes < kRotateBytes) return;
    std::fclose(g_file);
    g_file = nullptr;
    std::error_code ec;
    const std::filesystem::path current(g_path);
    std::filesystem::path previous = current;
    previous += L".1";
    std::filesystem::remove(previous, ec);
    std::filesystem::rename(current, previous, ec);
    g_file = _wfopen(g_path.c_str(), L"ab");
    g_bytes = 0;
}
} // namespace

void set_level(int level) { g_level.store(level); }
int level() { return g_level.load(); }

bool enabled(int level) { return level <= g_level.load() && level != ESP_LOG_NONE; }

bool open_file(const std::wstring &path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    g_file = _wfopen(path.c_str(), L"ab");
    if (!g_file) return false;
    g_path = path;
    g_bytes = size_t(std::filesystem::file_size(p, ec));
    if (ec) g_bytes = 0;
    return true;
}

void set_console(bool on) { g_console = on; }

void flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) std::fflush(g_file);
    if (g_console) std::fflush(stderr);
}

void close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}

size_t lines_written() { return g_lines.load(); }

void vwrite(int level, const char *tag, const char *format, va_list args) {
    char message[1024];
    const int n = std::vsnprintf(message, sizeof(message), format, args);
    if (n < 0) return;
    char line[1200];
    const long long ms = (long long)(esp_timer_get_time() / 1000);
    const int len = std::snprintf(line, sizeof(line), "%10lld %c (%s) %s\n", ms, level_letter(level),
                                  tag ? tag : "-", message);
    if (len <= 0) return;
    const size_t bytes = std::min(size_t(len), sizeof(line) - 1);
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_lines;
    if (g_file) {
        std::fwrite(line, 1, bytes, g_file);
        g_bytes += bytes;
        if (level <= ESP_LOG_WARN) std::fflush(g_file);
        rotate_locked();
    }
    if (g_console) std::fwrite(line, 1, bytes, stderr);
}

void write(int level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vwrite(level, tag, format, args);
    va_end(args);
}

} // namespace uv::log
