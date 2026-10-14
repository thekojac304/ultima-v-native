// Ultima V Native for Windows -- the force-include probe.
//
// Compiled with "/FI<esp_compat>/uv_storage_redirect.h" (tests/CMakeLists.txt),
// exactly like the shared device sources in uv_shared_app. Every call below is
// spelled the way native/targets/tdeck/main writes it (alpha_save.cpp,
// device_smoke_tests.cpp, tdeck_board.cpp): plain fopen / std::fopen, mkdir(p,
// 0777), struct stat + stat(p, &st) + S_ISDIR/S_ISREG, fsync(fileno(f)),
// unlink, rename / std::rename. If the force-include or a macro were missing,
// these would reach the C runtime with "/sd/..." and act on <drive>:\sd.
#if !defined(fopen) || !defined(rename) || !defined(unlink) || !defined(mkdir) || !defined(stat) || !defined(fsync) || \
    !defined(fileno)
#error "uv_storage_redirect.h must be force-included into this file (see tests/CMakeLists.txt)"
#endif

#include "storage_redirect_probe.h"

namespace {
bool write_text(const char *path, const char *text, size_t n) {
    FILE *f = std::fopen(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text, 1, n, f) == n && std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    return std::fclose(f) == 0 && ok;
}
} // namespace

RedirectProbeResult run_redirect_probe() {
    RedirectProbeResult r;
    r.force_included = true;

    // device_smoke_tests.cpp / alpha_save.cpp make_directory()
    errno = 0;
    r.mkdir_root = mkdir("/sd/ultima5", 0777) == 0 || errno == EEXIST;
    r.mkdir_sub = mkdir("/sd/ultima5/sub", 0777) == 0;
    errno = 0;
    r.mkdir_again_eexist = mkdir("/sd/ultima5/sub", 0777) != 0 && errno == EEXIST;

    // alpha_save.cpp write_file(): FILE* f = std::fopen(...); fwrite; fflush; fsync(fileno(f)); fclose
    FILE *f = fopen("/sd/ultima5/probe.txt", "wb");
    r.fopen_write = f != nullptr;
    if (f) {
        r.write_flush_fsync = std::fwrite("probe-data", 1, 10, f) == 10 && std::fflush(f) == 0 && fsync(fileno(f)) == 0;
        r.fclose_ok = std::fclose(f) == 0;
    }

    // alpha_save.cpp file_size() / make_directory()
    struct stat st {};
    r.stat_file = stat("/sd/ultima5/probe.txt", &st) == 0 && S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && st.st_size == 10;
    struct stat sd {};
    r.stat_dir = stat("/sd/ultima5/sub", &sd) == 0 && S_ISDIR(sd.st_mode) && !S_ISREG(sd.st_mode);

    // alpha_save.cpp read_file()
    if (FILE *g = std::fopen("/sd/ultima5/probe.txt", "rb")) {
        char buf[16]{};
        const size_t n = std::fread(buf, 1, sizeof buf, g);
        std::fclose(g);
        r.std_fopen_read = n == 10 && std::memcmp(buf, "probe-data", 10) == 0;
    }

    // alpha_save.cpp move_temp(): unlink(to); rename(from, to). First without
    // the unlink: FATFS refuses to rename onto an existing file.
    if (write_text("/sd/ultima5/probe.tmp", "new", 3)) {
        errno = 0;
        const int blocked = std::rename("/sd/ultima5/probe.tmp", "/sd/ultima5/probe.txt");
        r.rename_errno = errno;
        r.rename_blocked = blocked != 0 && (errno == EEXIST || errno == EACCES);
        r.unlink_then_rename = unlink("/sd/ultima5/probe.txt") == 0 && rename("/sd/ultima5/probe.tmp", "/sd/ultima5/probe.txt") == 0;
        struct stat after {};
        r.renamed_content = stat("/sd/ultima5/probe.txt", &after) == 0 && after.st_size == 3 &&
                            stat("/sd/ultima5/probe.tmp", &after) != 0;
    }

    r.unlink_final = unlink("/sd/ultima5/probe.txt") == 0;
    errno = 0;
    r.unlink_missing_enoent = unlink("/sd/ultima5/probe.txt") != 0 && errno == ENOENT;
    errno = 0;
    struct stat missing {};
    r.stat_missing_enoent = stat("/sd/ultima5/no-such-file.bin", &missing) != 0 && errno == ENOENT;
    return r;
}
