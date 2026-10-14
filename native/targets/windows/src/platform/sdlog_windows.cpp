// Ultima V Native for Windows: the SD diagnostic-log API of the shared runtime
// (native/targets/tdeck/main/sd_diagnostic_logger.h).
//
// On the T-Deck a FreeRTOS writer copies ESP_LOG lines to the card while the
// Developer switch is on, and a storage mutex keeps that writer off the card
// while a save transaction owns it. On Windows every log line already goes to
// the platform log file (src/platform/log.cpp), so the switch has nothing to
// enable: the state is reported as Unavailable (the Developer row then says
// the card log is not available) and the storage transaction is a plain mutex,
// keeping the save path's begin/end pairing meaningful.
#include "sd_diagnostic_logger.h"

#include <mutex>

namespace tdeck::sdlog {
namespace {
std::recursive_mutex g_storage; // a save may nest an inspect inside its own window
volatile uint32_t g_burst = 0;
} // namespace

bool begin_capture() { return false; }
bool initialize_storage() { return false; }
bool start_writer() { return false; }
openu5::SdLogState state() { return openu5::SdLogState::Unavailable; }
bool set_enabled(bool) { return false; }
const volatile uint32_t *burst_flag() { return &g_burst; }

bool begin_storage_transaction() {
    g_storage.lock();
    return true;
}

void end_storage_transaction() { g_storage.unlock(); }

bool perf_snapshot(openu5::SdLogPerf &out) {
    out = openu5::SdLogPerf{};
    out.valid = true;
    out.off = true;
    return true;
}

void perf_reset() {}

} // namespace tdeck::sdlog
