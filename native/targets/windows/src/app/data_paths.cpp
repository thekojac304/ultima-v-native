#include "app/data_paths.h"

#include <filesystem>

#include <windows.h>
#include <shlobj.h>

namespace uv {

std::wstring executable_directory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
        if (n == 0) return L".";
        if (n < buffer.size()) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path().wstring();
}

std::wstring user_data_directory() {
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &path)) && path) {
        out = std::wstring(path) + L"\\Ultima V Native";
    }
    if (path) CoTaskMemFree(path);
    return out;
}

DataPaths resolve_data_paths(const std::wstring &override_dir) {
    DataPaths paths;
    paths.executable_dir = executable_directory();
    std::error_code ec;
    if (!override_dir.empty()) {
        paths.data_dir = std::filesystem::absolute(std::filesystem::path(override_dir), ec).wstring();
        if (ec) paths.data_dir = override_dir;
        paths.source = DataDirSource::CommandLine;
    } else if (std::filesystem::exists(std::filesystem::path(paths.executable_dir) / L"portable.txt", ec)) {
        paths.data_dir = paths.executable_dir + L"\\data";
        paths.source = DataDirSource::Portable;
    } else {
        paths.data_dir = user_data_directory();
        if (paths.data_dir.empty()) {
            // No resolvable profile folder: fall back to portable behaviour.
            paths.data_dir = paths.executable_dir + L"\\data";
            paths.source = DataDirSource::Portable;
        } else {
            paths.source = DataDirSource::UserProfile;
        }
    }
    while (!paths.data_dir.empty() && (paths.data_dir.back() == L'\\' || paths.data_dir.back() == L'/'))
        paths.data_dir.pop_back();
    return paths;
}

const char *data_dir_source_name(DataDirSource source) {
    switch (source) {
    case DataDirSource::CommandLine: return "command line";
    case DataDirSource::Portable: return "portable";
    case DataDirSource::UserProfile: return "user profile";
    }
    return "?";
}

} // namespace uv
