#include "esphome/components/waveshare_box_rtc/rtc.h"
#include "openathan/scheduler.h"
#include <cassert>
#include <cstring>

using esphome::time::system_epoch;
using esphome::waveshare_box_rtc::RTC;
static int64_t epoch(openathan::CivilDate day, int h = 0, int m = 0, int s = 0) {
  return int64_t(openathan::day_number(day)) * 86400 + h * 3600 + m * 60 + s;
}
static constexpr int64_t SAMPLE = 1791547445; // 2026-10-09 12:04:05 UTC, Friday.
struct Fixture {
  esphome::time::RealTimeClock network;
  RTC rtc;
  Fixture() {
    sntp_test::reset();
    system_epoch = 0;
    rtc.set_network_time(&network);
    rtc.registers = {1, 0xC3, 0x8A, 0xA7, 0x05, 0x04, 0x12, 0x09, 5, 0x10, 0x26,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86};
  }
  void start() { rtc.setup(); }
  void sync(int64_t value = SAMPLE) { network.network_sync(value); }
};
static void restore() {
  Fixture f; f.start(); assert(system_epoch == SAMPLE && !std::strcmp(f.rtc.restore_result(), "restored"));
  assert(f.rtc.writes.empty());
  f.rtc.registers[4] = 0x30; f.rtc.update(); assert(system_epoch == SAMPLE); // no periodic restore
  Fixture network_first; system_epoch = SAMPLE + 3600; network_first.start();
  assert(network_first.rtc.reads == 0 && !std::strcmp(network_first.rtc.restore_result(), "system_valid"));
  Fixture race; race.rtc.on_read = [](unsigned, uint8_t *) { system_epoch = SAMPLE + 3600; };
  race.start(); assert(system_epoch == SAMPLE + 3600 && !std::strcmp(race.rtc.restore_result(), "system_valid"));
  Fixture missing; missing.rtc.fail_read_at = 1; missing.start();
  assert(system_epoch == 0 && !std::strcmp(missing.rtc.restore_result(), "io_error"));
  missing.sync(); assert(system_epoch == SAMPLE && !missing.rtc.is_failed()); // RTC remains optional
  Fixture vendor; vendor.rtc.registers[3] = 0; vendor.rtc.registers[10] = 0x56; vendor.start();
  assert(system_epoch == 0 && !std::strcmp(vendor.rtc.restore_result(), "uninitialized"));
}
static void invalid() {
  for (const auto [reg, value] : std::vector<std::pair<unsigned, uint8_t>>{
      {0,0x21}, {0,0x81}, {0,0x11}, {0,0x03}, {4,0x85}, {4,0x1A}, {4,0x60},
      {5,0x60}, {6,0x24}, {7,0x00}, {7,0x32}, {8,7}, {8,4}, {9,0x13}, {10,0x18}, {10,0xFA}}) {
    Fixture f; f.rtc.registers[reg] = value; f.start();
    assert(system_epoch == 0 && !std::strcmp(f.rtc.restore_result(), "invalid") && f.rtc.writes.empty());
  }
  Fixture feb; feb.rtc.registers[7] = 0x29; feb.rtc.registers[9] = 0x02; feb.start();
  assert(system_epoch == 0); // 2026 is not a leap year
}
static void saves() {
  Fixture f; f.start(); const auto before = f.rtc.registers; f.sync();
  assert(!std::strcmp(f.rtc.save_result(), "verified") && f.rtc.registers[3] == 0xA7);
  assert(f.rtc.writes.size() == 5 && f.rtc.writes.front().reg == 3 && f.rtc.writes.front().data[0] == 0);
  assert(f.rtc.writes[1].reg == 0 && f.rtc.writes[1].data[0] == 0x21);
  assert(f.rtc.writes.back().reg == 3 && f.rtc.writes.back().data[0] == 0xA7);
  f.network.time_sync_callback_.call();
  assert(f.rtc.writes.size() == 5); // Completion is consumed; duplicate notification is harmless.
  for (unsigned i : {1u, 2u, 11u, 12u, 13u, 14u, 15u, 16u, 17u}) assert(f.rtc.registers[i] == before[i]);
  // Sunday=0, leap day, midnight, upper supported year; each survives restart.
  for (auto value : {epoch({2028,2,29},23,59,59), epoch({2026,10,11}), epoch({2099,12,31},23,59,59)}) {
    f.sync(value); assert(!std::strcmp(f.rtc.save_result(), "verified"));
    Fixture restart; restart.rtc.registers = f.rtc.registers; restart.start(); assert(system_epoch == value);
  }
  Fixture unsupported; unsupported.start(); unsupported.sync(epoch({2100,1,1}));
  assert(unsupported.rtc.writes.empty() && !std::strcmp(unsupported.rtc.save_result(), "time_invalid"));
  Fixture recover; recover.rtc.registers[3] = 0; recover.start(); recover.sync();
  assert(!std::strcmp(recover.rtc.save_result(), "verified"));
}
static void failures() {
  for (unsigned failure = 1; failure <= 5; ++failure) {
    Fixture f; f.start(); f.rtc.fail_write_at = failure; f.sync(SAMPLE + 3600);
    assert(system_epoch == SAMPLE + 3600 && !std::strcmp(f.rtc.save_result(), "io_error"));
    if (failure > 1) {
      Fixture reboot; reboot.rtc.registers = f.rtc.registers; reboot.start(); assert(system_epoch == 0);
    }
    f.rtc.fail_write_at = 0; f.sync(); assert(!std::strcmp(f.rtc.save_result(), "verified"));
  }
  for (unsigned failure : {2u,3u,4u}) {
    Fixture f; f.start(); f.rtc.fail_read_at = failure; f.sync();
    assert(system_epoch == SAMPLE && !std::strcmp(f.rtc.save_result(), "io_error"));
    if (failure == 3) assert(f.rtc.registers[3] == 0); // verification failed before commit
  }
  Fixture mismatch; mismatch.start();
  mismatch.rtc.on_read = [](unsigned call, uint8_t *data) { if (call == 3) data[4] = 0x10; };
  mismatch.sync(); assert(system_epoch == SAMPLE && mismatch.rtc.registers[3] == 0);
  assert(!std::strcmp(mismatch.rtc.save_result(), "readback_error"));
  Fixture lost_marker; lost_marker.start();
  lost_marker.rtc.on_read = [](unsigned call, uint8_t *data) { if (call == 4) data[0] = 0; };
  lost_marker.sync(); assert(!std::strcmp(lost_marker.rtc.save_result(), "readback_error"));
}
struct Clock : openathan::Clock {
  uint64_t mono{};
  openathan::ClockSample read() override {
    return {system_epoch >= 1546300800, system_epoch, mono,
        openathan::civil_date(int32_t(system_epoch / 86400)), 0};
  }
  void step() { ++system_epoch; mono += 1000; }
};
struct Calculator : openathan::DayCalculator {
  bool calculate(const openathan::Settings &, openathan::CivilDate day, openathan::PrayerDay &out) override {
    constexpr int hours[]{5, 6, 12, 15, 18, 20};
    for (unsigned i = 0; i < 6; ++i) out[i] = epoch(day, hours[i]);
    return true;
  }
};
struct Store : openathan::StateStore {
  openathan::DurableState state;
  int writes{};
  openathan::LoadResult load(openathan::DurableState &out) override { out = state; return openathan::LoadResult::LOADED; }
  bool save(const openathan::DurableState &value) override { state = value; ++writes; return true; }
};
struct Audio : openathan::Playback {
  int starts{};
  bool ready() const override { return true; }
  bool playing() const override { return false; }
  bool start(openathan::Track) override { ++starts; return true; }
  void stop() override {}
};
static void scheduler_handoff() {
  Calculator calculator; Store store; Audio audio;
  Fixture initialized; initialized.start(); initialized.sync(epoch({2026,10,9},14,59,59));
  Fixture boot; boot.rtc.registers = initialized.rtc.registers; boot.start();
  Clock clock; openathan::Scheduler scheduler(clock, calculator, store, audio);
  assert(scheduler.begin({})); scheduler.tick(); assert(scheduler.status().automatic_ready && audio.starts == 0);
  clock.step(); scheduler.tick(); assert(audio.starts == 1); // upcoming offline Asr
  const auto consumed = store.state;
  // SNTP moving backwards across an already consumed boundary cannot replay it.
  boot.sync(epoch({2026,10,9},14,59,59)); scheduler.tick();
  clock.step(); scheduler.tick(); assert(audio.starts == 1 && store.state.consumed_through == consumed.consumed_through);
  // A forward correction across Maghrib cannot produce catch-up playback.
  boot.sync(epoch({2026,10,9},18,0,10)); scheduler.tick(); assert(audio.starts == 1);
  boot.sync(epoch({2026,10,9},19,59,59)); scheduler.tick();
  assert(scheduler.skip_next()); const auto skipped = store.state;
  Fixture reset; reset.rtc.registers = boot.rtc.registers; reset.start();
  Clock restarted_clock; openathan::Scheduler restarted(restarted_clock, calculator, store, audio);
  assert(restarted.begin({})); restarted.tick();
  assert(store.state.skip == skipped.skip && store.state.consumed_through == skipped.consumed_through);
  restarted_clock.step(); restarted.tick(); assert(audio.starts == 1 && !store.state.skip);
  // New local date remains schedulable after the saved skip is consumed.
  system_epoch = epoch({2026,10,10},4,59,59); restarted.tick();
  restarted_clock.step(); restarted.tick(); assert(audio.starts == 2);
  Fixture invalid_rtc; invalid_rtc.rtc.registers[4] |= 0x80; invalid_rtc.start();
  Clock waiting_clock; openathan::Scheduler waiting(waiting_clock, calculator, store, audio);
  assert(waiting.begin({})); const auto writes = store.writes; waiting.tick();
  assert(!waiting.status().clock_ready && store.writes == writes && audio.starts == 2);
}
int main() { restore(); invalid(); saves(); failures(); scheduler_handoff(); }
