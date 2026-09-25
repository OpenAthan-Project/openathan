#include "openathan/settings.h"
#include "nvs_settings_store.h"
#include "nvs_state_store.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using namespace openathan;
using namespace esphome::openathan_component;
static DeviceSettings defaults() {
  DeviceSettings s;
  s.prayer.latitude = 43.6532; s.prayer.longitude = -79.3832;
  s.prayer.method = Method::NORTH_AMERICA;
  s.timezone = {"America/Toronto", 18000, 14400,
      {7200, 0, DstRuleType::MONTH_WEEK_DAY, 3, 2, 0}, {7200, 0, DstRuleType::MONTH_WEEK_DAY, 11, 1, 0}};
  return s;
}
static auto preflight = [](const DeviceSettings &) { return true; };
static void encoding() {
  auto s = defaults(); s.prayer.offsets = {-120, -1, 0, 1, 60, 120};
  s.prayer.enabled = {false,true,false,true,false}; s.prayer.hanafi = true; s.volume = 0;
  for (unsigned method = 0; method < 12; ++method) {
    s.prayer.method = static_cast<Method>(method);
    for (unsigned latitude = 0; latitude < 4; ++latitude) {
      s.prayer.high_latitude = static_cast<HighLatitudeRule>(latitude);
      const SavedSettings original{42, s}; SavedSettings decoded;
      const auto bytes = encode_settings(original);
      CHECK(decode_settings(bytes, decoded) && decoded == original);
      for (size_t i = 0; i < bytes.size(); ++i) {
        auto corrupt = bytes; corrupt[i] ^= 1;
        CHECK(!decode_settings(corrupt, decoded));
      }
    }
  }
  s = defaults(); s.prayer.latitude = std::numeric_limits<double>::quiet_NaN(); CHECK(!valid_device_settings(s));
  s = defaults(); s.prayer.longitude = 181; CHECK(!valid_device_settings(s));
  s = defaults(); s.volume = 101; CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.start.month = 13; CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.end = {}; CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.standard_offset = 86401; CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.start.time_seconds = INT32_MIN; CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.name = std::string(97,'x'); CHECK(!valid_device_settings(s));
  s = defaults(); s.timezone.name = "bad\nname"; CHECK(!valid_device_settings(s));
  s = defaults(); s.prayer.offsets[0] = 121;
  SavedSettings out; CHECK(!decode_settings(encode_settings({1,s}), out));
  CHECK(!decode_settings(encode_settings({0,defaults()}), out));
  // Unsupported versions must fail even when their checksum is valid.
  auto unsupported = encode_settings({1, defaults()});
  unsupported[3] = '2';
  uint32_t checksum = 0xFFFFFFFF;
  for (size_t i = 0; i < unsupported.size()-4; ++i) {
    checksum ^= unsupported[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      checksum = (checksum >> 1) ^ ((checksum & 1) ? 0xEDB88320 : 0);
  }
  checksum = ~checksum;
  for (unsigned i = 0; i < 4; ++i)
    unsupported[unsupported.size()-4+i] = static_cast<uint8_t>(checksum >> (8*i));
  CHECK(!decode_settings(unsupported, out));
}
static void migration_and_validation() {
  nvs_test::reset();
  NvsStateStore history;
  DurableState state; CHECK(history.load(state) == LoadResult::EMPTY);
  state.consumed_through[0] = day_number({2026,9,25}); CHECK(history.save(state));
  const auto original_history = nvs_test::committed.at({"openathan", "scheduler"});
  NvsSettingsStore store; SettingsService service(store);
  CHECK(service.begin(defaults())); CHECK(service.saved()->revision == 1);
  auto changed = defaults(); changed.volume = 35; changed.prayer.hanafi = true;
  CHECK(service.update(changed, 2, preflight) == SettingsResult::CONFLICT);
  CHECK(service.update(changed, 1, [](const auto &) {return false;}) == SettingsResult::INVALID_SCHEDULE);
  auto invalid = changed; invalid.prayer.offsets[2] = 121;
  CHECK(service.update(invalid, 1, preflight) == SettingsResult::INVALID);
  CHECK(service.saved()->value == defaults());
  CHECK(service.update(changed, 1, preflight) == SettingsResult::SAVED);
  const auto writes = nvs_test::writes;
  CHECK(service.update(changed, 2, preflight) == SettingsResult::UNCHANGED && nvs_test::writes == writes);
  nvs_test::power_cycle(); NvsSettingsStore restored_store; SettingsService restored(restored_store);
  CHECK(restored.begin(defaults()) && restored.saved()->value == changed && restored.saved()->revision == 2);
  CHECK(nvs_test::committed.at({"openathan", "scheduler"}) == original_history);
  NvsSettingsStore isolated("oa_validation"); SettingsService test(isolated);
  CHECK(test.begin(defaults())); CHECK(test.update(defaults(), 1, preflight) == SettingsResult::UNCHANGED);
  CHECK(restored.saved()->value == changed);
}
static void power_loss() {
  for (unsigned point = 0; point < 4; ++point) for (bool early : {false,true}) {
    nvs_test::reset();
    NvsSettingsStore store; SettingsService service(store); CHECK(service.begin(defaults()));
    const auto old = *service.saved(); auto changed = defaults(); changed.volume = 31; changed.prayer.enabled[4] = false;
    nvs_test::early_persist = early;
    nvs_test::fail_write = point == 0; nvs_test::cut_after_write = point == 1;
    nvs_test::fail_commit = point == 2; nvs_test::cut_after_commit = point == 3;
    try {
      CHECK(service.update(changed, 1, preflight) == SettingsResult::STORAGE);
      CHECK(!service.healthy() && service.saved() == old);
      CHECK(service.update(changed, 1, preflight) == SettingsResult::STORAGE);
    } catch (const std::runtime_error &) { CHECK(point == 1 || point == 3); }
    nvs_test::power_cycle();
    nvs_test::fail_write = nvs_test::cut_after_write = nvs_test::fail_commit = nvs_test::cut_after_commit = false;
    NvsSettingsStore recovered_store; SettingsService recovered(recovered_store);
    CHECK(recovered.begin(defaults()));
    const auto expected = (point == 3 || (point != 0 && early)) ? SavedSettings{2,changed} : old;
    CHECK(recovered.saved() == expected);
  }
  // Loss after acknowledgment must always preserve the new complete record.
  nvs_test::reset(); NvsSettingsStore store; SettingsService service(store); CHECK(service.begin(defaults()));
  auto changed = defaults(); changed.volume = 99;
  CHECK(service.update(changed,1,preflight) == SettingsResult::SAVED);
  nvs_test::power_cycle(); NvsSettingsStore reboot_store; SettingsService reboot(reboot_store);
  CHECK(reboot.begin(defaults()) && reboot.saved()->value == changed);
}
static void storage_errors() {
  for (int failure = 0; failure < 6; ++failure) {
    nvs_test::reset(); NvsSettingsStore store; SettingsService service(store);
    CHECK(service.begin(defaults()));
    auto &bytes = nvs_test::committed.at({"openathan", "settings"});
    if (failure == 0) bytes.resize(1);
    if (failure == 1) bytes.resize(300);
    if (failure == 2) bytes[0] ^= 1;
    if (failure == 3) bytes[170] = 1;
    if (failure == 4) nvs_test::fail_read = true;
    if (failure == 5) nvs_test::fail_open = true;
    const auto writes = nvs_test::writes;
    NvsSettingsStore failed_store; SettingsService failed(failed_store);
    CHECK(!failed.begin(defaults()) && !failed.saved() && nvs_test::writes == writes);
  }
}
int main() { encoding(); migration_and_validation(); power_loss(); storage_errors(); }
