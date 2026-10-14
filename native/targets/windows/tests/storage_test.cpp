// Ultima V Native for Windows -- the "/sd" mount (src/platform/storage.*).
//
// For two data directories (an ASCII temp folder and one with non-ASCII
// characters, as under a user profile like C:\Users\Zoë): set_root, map_path,
// the UTF-8 <-> UTF-16 helpers, then the redirect functions the force-included
// header routes the device sources to (uv_fopen / uv_rename / uv_unlink /
// uv_mkdir / uv_stat / uv_fsync / uv_fileno) with FATFS semantics, the
// ESP-IDF mount and free-space calls, and finally the force-include probe
// (storage_redirect_probe.cpp), which uses the plain names through the macros.
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include "esp_vfs_fat.h"
#include "platform/storage.h"
#include "storage_redirect_probe.h"
#include "test_check.h"

// The declarations under test. The header also defines the redirect macros
// for force-included sources; this file calls the uv_* names explicitly, so
// the macros are removed again right away.
#include "uv_storage_redirect.h"
#undef fopen
#undef rename
#undef unlink
#undef mkdir
#undef stat
#undef fsync
#undef fileno

using uvtest::check;
using uvtest::format;
namespace fs = std::filesystem;

namespace {

fs::path unique_temp_dir(const std::wstring &leaf) {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH + 1, temp);
    fs::path base = n ? fs::path(std::wstring(temp, n)) : fs::temp_directory_path();
    fs::path dir = base / (leaf + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    std::error_code ec;
    fs::remove_all(dir, ec);
    return dir; // not created: the mount call must create it
}

std::vector<uint8_t> pattern(size_t n, uint8_t seed) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = uint8_t(seed + i * 7 + (i >> 8));
    return v;
}

bool write_file(const char *path, const std::vector<uint8_t> &bytes, bool use_fsync = true) {
    std::FILE *f = uv_fopen(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size() && std::fflush(f) == 0;
    if (use_fsync) ok = ok && uv_fsync(uv_fileno(f)) == 0;
    return std::fclose(f) == 0 && ok;
}

bool read_file(const char *path, std::vector<uint8_t> &out) {
    std::FILE *f = uv_fopen(path, "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) out.insert(out.end(), buf, buf + n);
    return std::fclose(f) == 0;
}

void helpers() {
    const std::string utf8 = "uv-t\xc3\xa9st-\xce\xa9 \xe2\x82\xac \xf0\x9f\x98\x80";
    const std::wstring wide = L"uv-t\u00e9st-\u03a9 \u20ac \U0001F600";
    check(uv::storage::utf8_to_wide(utf8) == wide, "H1 utf8_to_wide decodes 2-, 3- and 4-byte sequences (surrogate pair)");
    check(uv::storage::wide_to_utf8(wide) == utf8, "H2 wide_to_utf8 is its inverse");
    check(uv::storage::utf8_to_wide("").empty() && uv::storage::wide_to_utf8(L"").empty(), "H3 empty strings stay empty");
}

void mapping(const std::wstring &root, const char *tag) {
    uv::storage::set_root(root + L"\\\\");
    check(uv::storage::root() == root, format("M1 %s: set_root strips trailing separators", tag));
    uv::storage::set_root(root + L"/");
    check(uv::storage::root() == root, format("M2 %s: ... forward slashes too", tag));
    uv::storage::set_root(root);
    struct Row {
        const char *in;
        std::wstring want;
    };
    const Row rows[] = {
        {"/sd", root},
        {"/sd/", root + L"\\"},
        {"/sd/ultima5", root + L"\\ultima5"},
        {"/sd/ultima5/saves/alpha1-g0.gam", root + L"\\ultima5\\saves\\alpha1-g0.gam"},
        {"/sd/ultima5/t\xc3\xa9st-\xce\xa9.bin", root + L"\\ultima5\\t\u00e9st-\u03a9.bin"},
        {"/sdcard/x.bin", L"\\sdcard\\x.bin"},
        {"/sdx", L"\\sdx"},
        {"/s", L"\\s"},
        {"/SD/ultima5", L"\\SD\\ultima5"},
        {"C:/temp/a.txt", L"C:\\temp\\a.txt"},
        {"relative/dir/file", L"relative\\dir\\file"},
        {"", L""},
    };
    for (const Row &r : rows) {
        const std::wstring got = uv::storage::map_path(r.in);
        check(got == r.want, format("M3 %s: map_path(\"%s\")", tag, r.in),
              "got " + uv::storage::wide_to_utf8(got) + ", want " + uv::storage::wide_to_utf8(r.want));
    }
    check(uv::storage::map_path(nullptr).empty(), format("M4 %s: map_path(nullptr) is empty", tag));
}

void mount_calls(const fs::path &dir, const char *tag) {
    std::error_code ec;
    // No root: the mount refuses.
    uv::storage::set_root(L"");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdmmc_card_t *card = nullptr;
    check(esp_vfs_fat_sdspi_mount("/sd", &host, nullptr, nullptr, &card) == ESP_FAIL && card == nullptr,
          format("V1 %s: esp_vfs_fat_sdspi_mount fails while no root is set", tag));
    uint64_t total = 0, free_bytes = 0;
    check(esp_vfs_fat_info("/sd", &total, &free_bytes) == ESP_FAIL, format("V2 %s: esp_vfs_fat_info fails while no root is set", tag));

    uv::storage::set_root(dir.wstring());
    check(!fs::exists(dir, ec), format("V3 %s: the data directory does not exist before the mount", tag));
    check(esp_vfs_fat_sdspi_mount("/sdcard", &host, nullptr, nullptr, &card) == ESP_FAIL,
          format("V4 %s: only \"/sd\" is mounted (\"/sdcard\" fails)", tag));
    check(esp_vfs_fat_sdspi_mount(nullptr, &host, nullptr, nullptr, &card) == ESP_FAIL,
          format("V4 %s: a null base path fails", tag));
    host.max_freq_khz = 16000;
    const esp_err_t mounted = esp_vfs_fat_sdspi_mount("/sd", &host, nullptr, nullptr, &card);
    check(mounted == ESP_OK && card != nullptr, format("V5 %s: esp_vfs_fat_sdspi_mount(\"/sd\") succeeds once a root is set", tag),
          format("err=%d", int(mounted)));
    check(fs::is_directory(dir, ec), format("V6 %s: the mount created the data directory", tag));
    if (card) {
        check(card->csd.sector_size == 512 && card->csd.capacity > 0 && card->real_freq_khz == 16000 &&
                  (card->ocr & SD_OCR_SDHC_CAP) != 0,
              format("V7 %s: the card reports 512-byte sectors, a non-zero capacity, SDHC and the host clock", tag),
              format("sector=%u capacity=%u freq=%d ocr=%08x", unsigned(card->csd.sector_size), unsigned(card->csd.capacity),
                     card->real_freq_khz, unsigned(card->ocr)));
        std::printf("INFO sdmmc_card_print_info: ");
        sdmmc_card_print_info(stdout, card);
    }
    check(esp_vfs_fat_sdspi_mount("/sd", nullptr, nullptr, nullptr, nullptr) == ESP_OK,
          format("V8 %s: the mount tolerates null host and card pointers", tag));
    const esp_err_t info = esp_vfs_fat_info("/sd", &total, &free_bytes);
    check(info == ESP_OK && total > 0 && free_bytes > 0 && total >= free_bytes,
          format("V9 %s: esp_vfs_fat_info returns ESP_OK with total >= free > 0", tag),
          format("err=%d total=%llu free=%llu", int(info), (unsigned long long)total, (unsigned long long)free_bytes));
    uvtest::note("%s: volume total=%llu MiB free=%llu MiB", tag, (unsigned long long)(total >> 20),
                 (unsigned long long)(free_bytes >> 20));
    check(esp_vfs_fat_info("/sd", nullptr, nullptr) == ESP_OK, format("V10 %s: esp_vfs_fat_info accepts null outputs", tag));
    check(esp_vfs_fat_sdcard_unmount("/sd", card) == ESP_OK, format("V11 %s: unmount succeeds", tag));
}

void redirect_functions(const fs::path &dir, const char *tag) {
    std::error_code ec;
    uv::storage::set_root(dir.wstring());
    const fs::path ultima5 = dir / L"ultima5";

    errno = 0;
    check(uv_mkdir("/sd/ultima5", 0777) == 0 && fs::is_directory(ultima5, ec), format("R1 %s: uv_mkdir(\"/sd/ultima5\") creates <root>\\ultima5", tag));
    errno = 0;
    const int again = uv_mkdir("/sd/ultima5", 0777);
    check(again == -1 && errno == EEXIST, format("R2 %s: uv_mkdir of an existing directory fails with EEXIST", tag),
          format("ret=%d errno=%d", again, errno));
    errno = 0;
    const int deep = uv_mkdir("/sd/ultima5/missing-parent/child", 0777);
    check(deep == -1 && errno == ENOENT, format("R3 %s: uv_mkdir does not create parents (ENOENT), like FATFS", tag),
          format("ret=%d errno=%d", deep, errno));

    // Write and read through "/sd/ultima5/x.bin".
    const std::vector<uint8_t> data = pattern(70000, 3);
    check(write_file("/sd/ultima5/x.bin", data), format("R4 %s: uv_fopen(\"wb\") + fwrite + fflush + uv_fsync(uv_fileno) + fclose", tag));
    check(fs::is_regular_file(ultima5 / L"x.bin", ec) && fs::file_size(ultima5 / L"x.bin", ec) == data.size(),
          format("R5 %s: the bytes landed in <root>\\ultima5\\x.bin (70000 bytes)", tag));
    std::vector<uint8_t> back;
    check(read_file("/sd/ultima5/x.bin", back) && back == data, format("R6 %s: uv_fopen(\"rb\") reads the same bytes back", tag));

    // stat
    struct stat st {};
    check(uv_stat("/sd/ultima5/x.bin", &st) == 0 && S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && st.st_size == 70000,
          format("R7 %s: uv_stat on a file: S_ISREG and st_size 70000", tag), format("mode=%o size=%ld", unsigned(st.st_mode), long(st.st_size)));
    struct stat sd {};
    check(uv_stat("/sd/ultima5", &sd) == 0 && S_ISDIR(sd.st_mode) && !S_ISREG(sd.st_mode),
          format("R8 %s: uv_stat on a directory: S_ISDIR", tag), format("mode=%o", unsigned(sd.st_mode)));
    struct stat rt {};
    check(uv_stat("/sd", &rt) == 0 && S_ISDIR(rt.st_mode), format("R9 %s: uv_stat(\"/sd\") is the root directory", tag));
    errno = 0;
    struct stat ms {};
    const int missing = uv_stat("/sd/ultima5/none.bin", &ms);
    check(missing == -1 && errno == ENOENT, format("R10 %s: uv_stat of a missing file: -1, ENOENT", tag),
          format("ret=%d errno=%d", missing, errno));
    errno = 0;
    check(uv_stat("/sd/ultima5/x.bin", nullptr) == -1 && errno == EINVAL, format("R11 %s: uv_stat with a null buffer: EINVAL", tag));

    // rename: FATFS refuses an existing destination; the device unlinks first.
    const std::vector<uint8_t> fresh = pattern(1234, 99);
    check(write_file("/sd/ultima5/y.tmp", fresh, false), format("R12 %s: a second file y.tmp is written (no fsync)", tag));
    errno = 0;
    const int blocked = uv_rename("/sd/ultima5/y.tmp", "/sd/ultima5/x.bin");
    const int blocked_errno = errno;
    check(blocked == -1 && (blocked_errno == EEXIST || blocked_errno == EACCES),
          format("R13 %s: uv_rename onto an existing file fails (FATFS semantics), errno EEXIST or EACCES", tag),
          format("ret=%d errno=%d", blocked, blocked_errno));
    uvtest::note("%s: the blocked rename reported errno %d (%s)", tag, blocked_errno,
                 blocked_errno == EEXIST ? "EEXIST" : blocked_errno == EACCES ? "EACCES" : "other");
    check(read_file("/sd/ultima5/x.bin", back) && back == data && fs::exists(ultima5 / L"y.tmp", ec),
          format("R14 %s: after the refused rename both files are untouched", tag));
    check(uv_unlink("/sd/ultima5/x.bin") == 0 && !fs::exists(ultima5 / L"x.bin", ec), format("R15 %s: uv_unlink removes x.bin", tag));
    check(uv_rename("/sd/ultima5/y.tmp", "/sd/ultima5/x.bin") == 0, format("R16 %s: uv_rename succeeds once the destination is gone", tag));
    check(read_file("/sd/ultima5/x.bin", back) && back == fresh && !fs::exists(ultima5 / L"y.tmp", ec),
          format("R17 %s: x.bin now holds y.tmp's bytes and y.tmp is gone", tag));
    errno = 0;
    const int rename_missing = uv_rename("/sd/ultima5/none.tmp", "/sd/ultima5/none.bin");
    check(rename_missing == -1 && errno == ENOENT, format("R18 %s: uv_rename of a missing source: -1, ENOENT", tag),
          format("ret=%d errno=%d", rename_missing, errno));
    errno = 0;
    const int unlink_missing = uv_unlink("/sd/ultima5/none.bin");
    check(unlink_missing == -1 && errno == ENOENT, format("R19 %s: uv_unlink of a missing file: -1, ENOENT", tag),
          format("ret=%d errno=%d", unlink_missing, errno));

    // A non-ASCII file name under /sd.
    const char *nonascii = "/sd/ultima5/t\xc3\xa9st-\xce\xa9.bin";
    check(write_file(nonascii, pattern(10, 1)) && fs::is_regular_file(ultima5 / L"t\u00e9st-\u03a9.bin", ec),
          format("R20 %s: a UTF-8 file name under /sd becomes the same Unicode name on disk", tag));
    check(uv_unlink(nonascii) == 0, format("R21 %s: ... and is removed by its UTF-8 name", tag));

    // Paths outside "/sd" pass through unmapped.
    const std::string host_dir = uv::storage::wide_to_utf8((dir / L"host").wstring());
    check(uv_mkdir(host_dir.c_str(), 0777) == 0 && fs::is_directory(dir / L"host", ec),
          format("R22 %s: uv_mkdir with a host path (not /sd) creates it as given", tag));
    const std::string host_file = host_dir + "/plain.txt";
    check(write_file(host_file.c_str(), pattern(5, 5)) && fs::file_size(dir / L"host" / L"plain.txt", ec) == 5,
          format("R23 %s: uv_fopen with a host path (forward slashes) writes that file", tag));
    struct stat hs {};
    check(uv_stat(host_file.c_str(), &hs) == 0 && S_ISREG(hs.st_mode) && hs.st_size == 5,
          format("R24 %s: uv_stat with a host path", tag));
    check(uv_unlink(host_file.c_str()) == 0, format("R25 %s: uv_unlink with a host path", tag));

    // Argument errors.
    errno = 0;
    check(uv_fopen(nullptr, "rb") == nullptr && errno == EINVAL, format("R26 %s: uv_fopen(nullptr): EINVAL", tag));
    errno = 0;
    check(uv_fopen("/sd/ultima5/x.bin", nullptr) == nullptr && errno == EINVAL, format("R26 %s: uv_fopen with a null mode: EINVAL", tag));
    errno = 0;
    std::FILE *nodir = uv_fopen("/sd/ultima5/nodir/x.bin", "wb");
    check(nodir == nullptr && errno == ENOENT, format("R27 %s: uv_fopen(\"wb\") into a missing folder fails (ENOENT)", tag),
          format("errno=%d", errno));
    if (nodir) std::fclose(nodir);

    // uv_fileno / uv_fsync on a read-only stream and on stdout.
    if (std::FILE *ro = uv_fopen("/sd/ultima5/x.bin", "rb")) {
        const int fd = uv_fileno(ro);
        check(fd >= 0, format("R28 %s: uv_fileno returns a descriptor", tag), format("fd=%d", fd));
        std::fclose(ro);
    }

    // A file another handle holds open without FILE_SHARE_DELETE: the unlink
    // retries for antivirus-style locks, then gives up -- it must not hang.
    {
        const fs::path held = ultima5 / L"held.bin";
        check(write_file("/sd/ultima5/held.bin", pattern(16, 2)), format("R29 %s: held.bin written", tag));
        HANDLE h = CreateFileW(held.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        const auto t0 = std::chrono::steady_clock::now();
        errno = 0;
        const int r = uv_unlink("/sd/ultima5/held.bin");
        const int e = errno;
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        check(h != INVALID_HANDLE_VALUE && elapsed_ms < 3000,
              format("R30 %s: uv_unlink of a file held open elsewhere returns within 3 s (no hang)", tag),
              format("ret=%d errno=%d ms=%lld", r, e, (long long)elapsed_ms));
        uvtest::note("%s: unlink while held open: ret=%d errno=%d after %lld ms", tag, r, e, (long long)elapsed_ms);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        uv_unlink("/sd/ultima5/held.bin");
    }
    uv_unlink("/sd/ultima5/x.bin");
}

void probe(const fs::path &dir, const char *tag) {
    std::error_code ec;
    uv::storage::set_root(dir.wstring());
    const bool stray_before = fs::exists(L"\\sd\\ultima5\\sub", ec);
    const RedirectProbeResult r = run_redirect_probe();
    check(r.force_included, format("P0 %s: the probe TU was compiled with the force-included redirect", tag));
    check(r.mkdir_root, format("P1 %s: mkdir(\"/sd/ultima5\", 0777) == 0 || errno == EEXIST (through the macro)", tag));
    check(r.mkdir_sub, format("P2 %s: mkdir(\"/sd/ultima5/sub\", 0777) == 0", tag));
    check(r.mkdir_again_eexist, format("P3 %s: mkdir of the existing directory: -1, EEXIST", tag));
    check(r.fopen_write, format("P4 %s: plain fopen(\"/sd/ultima5/probe.txt\", \"wb\")", tag));
    check(r.write_flush_fsync && r.fclose_ok, format("P5 %s: fwrite + fflush + fsync(fileno(f)) + fclose", tag));
    check(r.stat_file, format("P6 %s: struct stat + stat(): S_ISREG, st_size 10", tag));
    check(r.stat_dir, format("P7 %s: stat(\"/sd/ultima5/sub\"): S_ISDIR", tag));
    check(r.std_fopen_read, format("P8 %s: std::fopen(\"rb\") reads the bytes back", tag));
    check(r.rename_blocked, format("P9 %s: std::rename onto an existing file fails (EEXIST/EACCES)", tag), format("errno=%d", r.rename_errno));
    check(r.unlink_then_rename && r.renamed_content, format("P10 %s: unlink(dst) then rename() -- the device's move_temp", tag));
    check(r.unlink_final, format("P11 %s: unlink() removes the file", tag));
    check(r.unlink_missing_enoent && r.stat_missing_enoent, format("P12 %s: unlink/stat of a missing file report ENOENT", tag));
    check(fs::is_directory(dir / L"ultima5" / L"sub", ec) && !fs::exists(dir / L"ultima5" / L"probe.txt", ec),
          format("P13 %s: the effects are in the data directory (<root>\\ultima5\\sub exists, probe.txt removed)", tag));
    check(stray_before || !fs::exists(L"\\sd\\ultima5\\sub", ec),
          format("P14 %s: nothing was created under <current drive>:\\sd (the calls really were redirected)", tag));
}

void run_root(const std::wstring &leaf, const char *tag) {
    const fs::path dir = unique_temp_dir(leaf);
    std::printf("INFO %s: data directory %s\n", tag, uv::storage::wide_to_utf8(dir.wstring()).c_str());
    mapping(dir.wstring(), tag);
    mount_calls(dir, tag);
    redirect_functions(dir, tag);
    probe(dir, tag);
    std::error_code ec;
    fs::remove_all(dir, ec);
    check(!fs::exists(dir, ec), format("Z1 %s: temp data directory removed", tag));
    uv::storage::set_root(L"");
}

} // namespace

int main() {
    std::printf("uv_storage_test: the /sd mount, the redirect functions and the force-included macros\n");
    helpers();
    run_root(L"uv-storage-test", "ASCII root");
    run_root(L"uv-t\u00e9st-\u03a9", "non-ASCII root (uv-t\u00e9st-\u03a9)");
    return uvtest::finish("uv_storage_test");
}
