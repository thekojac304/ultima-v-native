#pragma once
// Ultima V Native for Windows: where the game's data directory is.
//
// The data directory stands in for the T-Deck's SD card (src/platform/storage.h)
// and holds, under ultima5\, the resource packs, saves, settings.json, the PC
// save import/export folders and logs -- the SD card's own layout.
//
//   1. --data-dir <path> on the command line, else
//   2. portable mode: a file named "portable.txt" beside the executable
//      makes "<exe folder>\data" the data directory, else
//   3. the per-user "%APPDATA%\Ultima V Native" (FOLDERID_RoamingAppData).
#include <string>

namespace uv {

enum class DataDirSource { CommandLine, Portable, UserProfile };

struct DataPaths {
    std::wstring executable_dir;  // folder of UltimaVNative.exe
    std::wstring data_dir;        // what "/sd" maps to
    DataDirSource source = DataDirSource::UserProfile;
    std::wstring ultima5_dir() const { return data_dir + L"\\ultima5"; }
    std::wstring logs_dir() const { return ultima5_dir() + L"\\logs"; }
};

std::wstring executable_directory();
/** The per-user default, "%APPDATA%\Ultima V Native"; empty if the folder cannot be resolved. */
std::wstring user_data_directory();
/** Resolves the data directory; `override_dir` is the --data-dir value (may be empty). */
DataPaths resolve_data_paths(const std::wstring &override_dir);
const char *data_dir_source_name(DataDirSource);

} // namespace uv
