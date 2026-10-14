// Ultima V Native for Windows: process entry. See app/application.h.
#include <cstdio>
#include <string>

#include <windows.h>
#include <shellapi.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/application.h"

namespace {
// --console, or a start from a command prompt: write the log there too.
void attach_console() {
    // Output already redirected by whoever started us (a test runner, a pipe):
    // the C runtime has picked those handles up; leave them alone.
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err && err != INVALID_HANDLE_VALUE && GetFileType(err) != FILE_TYPE_UNKNOWN) return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole()) return;
    FILE *ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
}
} // namespace

int main(int, char **) {
    // The wide command line: data folders may have non-ASCII names.
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    uv::AppOptions options = uv::parse_options(argc > 1 ? argc - 1 : 0, argv ? argv + 1 : nullptr);
    if (argv) LocalFree(argv);
    if (options.console || !options.autotest_script.empty()) attach_console();
    if (options.show_help || !options.errors.empty()) {
        std::string text;
        for (const auto &e : options.errors) text += e + "\n";
        if (!text.empty()) text += "\n";
        text += uv::usage_text();
        std::fputs(text.c_str(), stderr);
        SDL_ShowSimpleMessageBox(options.errors.empty() ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_ERROR,
                                 "Ultima V Native", text.c_str(), nullptr);
        return options.errors.empty() ? 0 : 64;
    }
    return uv::run_application(options);
}
