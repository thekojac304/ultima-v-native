#pragma once
// Ultima V Native for Windows: the platform log behind ESP_LOGx (esp_compat/esp_log.h).
//
// Lines are "<ms since start> <level> (<tag>) <message>". They go to a file
// (rotated once at kRotateBytes, the previous one kept as <name>.1) and, when
// requested, to stderr. Writes are buffered; WARN and ERROR flush at once and
// everything else is flushed by flush() (the app calls it about once a second)
// and at shutdown, so the per-input INFO traces never stall a frame.
#include <cstddef>
#include <string>

namespace uv::log {

constexpr size_t kRotateBytes = 8u * 1024u * 1024u;

/** ESP_LOG_* threshold; messages above it are dropped before formatting. */
void set_level(int level);
int level();
/** Opens (appends to) `path`, creating its directory; false if it cannot. */
bool open_file(const std::wstring &path);
void set_console(bool on);
void flush();
void close();
/** How many lines were written / dropped by the level since start (tests). */
size_t lines_written();

} // namespace uv::log
