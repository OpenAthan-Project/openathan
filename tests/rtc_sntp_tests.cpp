#include "esphome/components/sntp/sntp_component.h"
#include "esphome/components/waveshare_box_rtc/rtc.h"
#include <cassert>
#include <cstring>

using esphome::time::system_epoch;
using esphome::waveshare_box_rtc::RTC;
constexpr time_t SAMPLE = 1791547445; // 2026-10-09 12:04:05 UTC.
struct Fixture {
  esphome::sntp::SNTPComponent network{{"unreachable", "unreachable", "unreachable"}};
  RTC rtc;
  Fixture() {
    sntp_test::reset();
    system_epoch = 0;
    rtc.set_network_time(&network);
    rtc.registers = {1, 0xC3, 0x8A, 0xA7, 0x05, 0x04, 0x12, 0x09, 5, 0x10, 0x26};
    rtc.setup();
    network.setup();
    assert(system_epoch == SAMPLE && rtc.writes.empty());
  }
  void deliver(time_t epoch) {
    // ESP-IDF updates UTC and marks completion before invoking its registered
    // notification. Production ESPHome then defers delivery to its main loop.
    system_epoch = epoch;
    sntp_test::status = SNTP_SYNC_STATUS_COMPLETED;
    timeval sample{epoch, 0};
    assert(sntp_test::notification);
    sntp_test::notification(&sample);
  }
};
static void offline_startup() {
  for (unsigned failure = 0; failure <= 5; ++failure) {
    Fixture f;
    const auto retained = f.rtc.registers;
    const auto reads = f.rtc.reads;
    f.rtc.fail_write_at = failure;
    f.network.loop(); // Execute the pinned production SNTP first-loop behavior.
    assert(f.network.loop_disabled && f.rtc.writes.empty() && f.rtc.reads == reads);
    assert(f.rtc.registers == retained && !std::strcmp(f.rtc.save_result(), "pending"));
    system_epoch = 0;
    RTC restarted;
    restarted.set_network_time(&f.network);
    restarted.registers = f.rtc.registers;
    restarted.setup();
    assert(system_epoch == SAMPLE && !std::strcmp(restarted.restore_result(), "restored"));
  }
}
static void network_updates() {
  for (bool before_first_loop : {false, true}) {
    Fixture f;
    if (!before_first_loop) f.network.loop();
    f.deliver(SAMPLE + 3600);
    if (before_first_loop) f.network.loop(); // Can consume completion before deferred notification.
    f.network.run_deferred();
    assert(f.rtc.writes.size() == 5 && !std::strcmp(f.rtc.save_result(), "verified"));
    f.network.time_synced();
    assert(f.rtc.writes.size() == 5); // Duplicate callback has no completion token.
    sntp_test::status = SNTP_SYNC_STATUS_IN_PROGRESS;
    f.network.time_synced();
    assert(f.rtc.writes.size() == 5); // Do not persist a clock still being corrected.
    f.deliver(SAMPLE + 7200);
    f.network.run_deferred();
    assert(f.rtc.writes.size() == 10 && !std::strcmp(f.rtc.save_result(), "verified"));
  }
}
static void genuine_write_failure() {
  Fixture f;
  f.network.loop();
  f.rtc.fail_write_at = 3;
  f.deliver(SAMPLE + 3600);
  f.network.run_deferred();
  assert(system_epoch == SAMPLE + 3600 && !std::strcmp(f.rtc.save_result(), "io_error"));
  assert(f.rtc.registers[3] == 0 && (f.rtc.registers[0] & 0x20));
  f.rtc.fail_write_at = 0;
  f.deliver(SAMPLE + 7200);
  f.network.run_deferred();
  assert(!std::strcmp(f.rtc.save_result(), "verified"));
}
int main() { offline_startup(); network_updates(); genuine_write_failure(); }
