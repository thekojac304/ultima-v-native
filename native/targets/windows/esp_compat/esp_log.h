#pragma once
// Ultima V Native for Windows: ESP_LOGx. On the device these go to the serial
// console (and, when switched on, the SD diagnostic log). On Windows they go
// to the platform log (src/platform/log.cpp): a rotating file under the data
// directory's logs folder, plus stderr when a console is attached. Every log
// call in the shared runtime is a diagnostic trace and never decides state.
//
// The level is checked before the arguments are formatted, so the per-input
// INFO traces cost nothing when the log level is WARN or lower.
#include <cstdarg>

#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5

#if defined(__GNUC__) || defined(__clang__)
#define UV_PRINTF_FORMAT(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
#define UV_PRINTF_FORMAT(fmt_index, first_arg)
#endif

namespace uv::log {
/** True when a message of `level` (ESP_LOG_*) would be written. */
bool enabled(int level);
void write(int level, const char *tag, const char *format, ...) UV_PRINTF_FORMAT(3, 4);
void vwrite(int level, const char *tag, const char *format, va_list args);
} // namespace uv::log

#define UV_ESP_LOG(level, tag, fmt, ...)                                                 \
    do {                                                                                 \
        if (::uv::log::enabled(level)) ::uv::log::write(level, tag, fmt __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)

#define ESP_LOGE(tag, fmt, ...) UV_ESP_LOG(ESP_LOG_ERROR, tag, fmt __VA_OPT__(, ) __VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) UV_ESP_LOG(ESP_LOG_WARN, tag, fmt __VA_OPT__(, ) __VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) UV_ESP_LOG(ESP_LOG_INFO, tag, fmt __VA_OPT__(, ) __VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) UV_ESP_LOG(ESP_LOG_DEBUG, tag, fmt __VA_OPT__(, ) __VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) UV_ESP_LOG(ESP_LOG_VERBOSE, tag, fmt __VA_OPT__(, ) __VA_ARGS__)
