#pragma once
// Ultima V Native for Windows -- the force-include probe (storage_test).
//
// storage_redirect_probe.cpp is compiled with esp_compat/uv_storage_redirect.h
// force-included (MSVC /FI, as uv_shared_app is) and calls the plain
// stdio/POSIX names exactly as the device sources do. This header only
// names the result, so it must not use any of the redirected identifiers.

struct RedirectProbeResult {
    bool force_included = false;      // the macros were in effect in that TU
    bool mkdir_root = false;          // mkdir("/sd/ultima5", 0777) == 0 || errno == EEXIST
    bool mkdir_sub = false;           // mkdir("/sd/ultima5/sub", 0777) == 0
    bool mkdir_again_eexist = false;  // the same mkdir again: -1, errno EEXIST
    bool fopen_write = false;         // fopen("/sd/ultima5/probe.txt", "wb")
    bool write_flush_fsync = false;   // fwrite + fflush + fsync(fileno(f)) == 0
    bool fclose_ok = false;
    bool stat_file = false;           // stat(): S_ISREG, st_size == 10
    bool stat_dir = false;            // stat("/sd/ultima5/sub"): S_ISDIR
    bool std_fopen_read = false;      // std::fopen(..., "rb") reads the same 10 bytes back
    bool rename_blocked = false;      // std::rename onto an existing file: -1, errno EEXIST or EACCES
    bool unlink_then_rename = false;  // unlink(dst) == 0, then rename() == 0 (the device's move_temp)
    bool renamed_content = false;     // the destination now holds the renamed file (3 bytes)
    bool unlink_final = false;        // unlink(...) == 0
    bool unlink_missing_enoent = false; // unlink of a missing file: -1, errno ENOENT
    bool stat_missing_enoent = false;   // stat of a missing file: -1, errno ENOENT
    int rename_errno = 0;             // what the blocked rename reported
};

/** Runs the probe against whatever directory "/sd" is mapped to (uv::storage::set_root). */
RedirectProbeResult run_redirect_probe();
