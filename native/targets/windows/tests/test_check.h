#pragma once
// Ultima V Native for Windows -- the Windows target's test helper.
//
// No framework: every check prints one line, "PASS <name>" or
// "FAIL <name>: <detail>", and the executable's exit code is the number of
// failed checks (0 = green), so ctest reads the result and the log says
// exactly what was checked. A test that needs the player's own game files
// prints "SKIP <name>: <why>" and does not count as a failure.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace uvtest {

struct Checks {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

inline Checks &checks() {
    static Checks c;
    return c;
}

/** One check: PASS/FAIL line; `detail` is printed only on failure. */
inline bool check(bool good, const std::string &name, const std::string &detail = std::string()) {
    if (good) {
        ++checks().passed;
        std::printf("PASS %s\n", name.c_str());
    } else {
        ++checks().failed;
        std::printf("FAIL %s: %s\n", name.c_str(), detail.empty() ? "condition is false" : detail.c_str());
    }
    std::fflush(stdout);
    return good;
}

inline void skip(const std::string &name, const std::string &why) {
    ++checks().skipped;
    std::printf("SKIP %s: %s\n", name.c_str(), why.c_str());
    std::fflush(stdout);
}

/** An informational line (values the log should carry, e.g. a hash to compare across compilers). */
inline void note(const char *format, ...) {
    std::printf("INFO ");
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::printf("\n");
    std::fflush(stdout);
}

inline std::string format(const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    return buffer;
}

inline std::string hex64(uint64_t v) { return format("%016llx", static_cast<unsigned long long>(v)); }

/** Prints the totals and returns the process exit code (the failure count, capped). */
inline int finish(const char *suite) {
    const Checks &c = checks();
    std::printf("%s: %d passed, %d failed, %d skipped\n", suite, c.passed, c.failed, c.skipped);
    std::fflush(stdout);
    return c.failed > 125 ? 125 : c.failed;
}

/** FNV-1a, 64-bit, byte-wise (the standard one). */
struct Fnv1a64 {
    uint64_t h = 1469598103934665603ull;
    void byte(uint8_t b) {
        h ^= b;
        h *= 1099511628211ull;
    }
    void bytes(const void *data, size_t n) {
        const auto *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < n; ++i) byte(p[i]);
    }
    void u16(uint16_t v) { // little-endian
        byte(uint8_t(v));
        byte(uint8_t(v >> 8));
    }
    void u32(uint32_t v) {
        u16(uint16_t(v));
        u16(uint16_t(v >> 16));
    }
    void u64(uint64_t v) {
        u32(uint32_t(v));
        u32(uint32_t(v >> 32));
    }
};

} // namespace uvtest
