#pragma once
// Ultima V Native for Windows: the "/sd" mount.
//
// On the T-Deck, ESP-IDF's VFS mounts the SD card at "/sd" and every file the
// shared runtime touches is a "/sd/ultima5/..." path: the resource packs, the
// save generations, settings.json, the PC save import/export folders and the
// logs. On Windows the same paths are resolved against a data directory (the
// "card"): "/sd/ultima5/saves/alpha1-g0.gam" -> "<root>\ultima5\saves\alpha1-g0.gam".
// The layout is therefore identical to the SD card's, so a card's ultima5
// folder can be copied into the data directory (and back) as it is.
//
// The device sources are compiled unchanged: uv_storage_redirect.h is
// force-included into them and routes their path-taking stdio/POSIX calls
// (fopen, rename, unlink, mkdir, stat, fsync) through the functions below,
// which map the path and use the wide-character Win32 API, so a data
// directory under a non-ASCII user profile works.
#include <cstdint>
#include <string>

namespace uv::storage {

/** The directory "/sd" maps to. Set once at start-up, before any file access. */
void set_root(const std::wstring &directory);
const std::wstring &root();

/**
 * Maps a path as the shared code writes it (UTF-8): "/sd" and "/sd/..." go
 * under root(); any other path is taken as a host path. Forward slashes are
 * converted to backslashes.
 */
std::wstring map_path(const char *path);

std::wstring utf8_to_wide(const std::string &text);
std::string wide_to_utf8(const std::wstring &text);

/** Creates `directory` and its parents; true if it exists afterwards. */
bool ensure_directory(const std::wstring &directory);

} // namespace uv::storage
