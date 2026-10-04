#include "openathan/time_format.h"
#include "nvs_settings_store.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
using namespace openathan;
namespace openathan_storage = esphome::openathan_storage;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
int main() {
  nvs_test::reset();
  esphome::openathan_component::NvsTimeFormatStore store;
  TimeFormatPreferences preferences(store); preferences.begin();
  CHECK(preferences.hours() == 24 && preferences.saved()->revision == 1 && nvs_test::writes == 0);
  CHECK(preferences.update(13, 1) == TimeFormatResult::INVALID);
  CHECK(preferences.update(12, 0) == TimeFormatResult::CONFLICT);
  CHECK(preferences.update(24, 1) == TimeFormatResult::UNCHANGED && nvs_test::writes == 0);
  nvs_test::committed[{openathan_storage::PRAYER, "settings"}] = {1,2,3};
  nvs_test::committed[{openathan_storage::PRAYER, "scheduler"}] = {4,5,6};
  const auto before = nvs_test::committed;
  CHECK(preferences.update(12, 1) == TimeFormatResult::SAVED);
  for (const auto &[key, value] : before) CHECK(nvs_test::committed.at(key) == value);
  CHECK(nvs_test::committed.contains({openathan_storage::PRAYER, "time_format"}));
  CHECK(!nvs_test::committed.contains({openathan_storage::TEST_MODE ? "openathan" : "oa_test", "time_format"}));
  nvs_test::power_cycle();
  esphome::openathan_component::NvsTimeFormatStore reboot_store;
  TimeFormatPreferences reboot(reboot_store); reboot.begin();
  CHECK(reboot.hours() == 12 && reboot.saved()->revision == 2);
  CHECK(reboot.update(24, 1) == TimeFormatResult::CONFLICT);
  nvs_test::fail_commit = true;
  CHECK(reboot.update(24, 2) == TimeFormatResult::STORAGE);
  CHECK(reboot.hours() == 12 && !reboot.writable());
  nvs_test::fail_commit = false; nvs_test::power_cycle();
  // Either permitted outcome of an interrupted write reloads a valid format.
  for (bool early : {false, true}) {
    nvs_test::reset();
    esphome::openathan_component::NvsTimeFormatStore interrupted_store;
    TimeFormatPreferences interrupted(interrupted_store); interrupted.begin();
    nvs_test::early_persist=early; nvs_test::cut_after_write=true;
    bool cut=false;
    try { interrupted.update(12,1); } catch (const std::runtime_error &) { cut=true; }
    CHECK(cut); nvs_test::power_cycle(); nvs_test::cut_after_write=false;
    esphome::openathan_component::NvsTimeFormatStore restored_store;
    TimeFormatPreferences restored(restored_store); restored.begin();
    CHECK(restored.writable() && restored.hours()==(early ? 12 : 24));
  }
  auto record = encode_time_format({2,12}); SavedTimeFormat decoded;
  CHECK(decode_time_format(record,decoded) && decoded.hours == 12);
  for (size_t i = 0; i < record.size(); ++i) {
    auto broken = record; broken[i] ^= 1; CHECK(!decode_time_format(broken,decoded));
  }
  CHECK(!decode_time_format(encode_time_format({2,13}),decoded));
  CHECK(!decode_time_format(encode_time_format({0,12}),decoded));
  nvs_test::committed[{openathan_storage::PRAYER,"time_format"}] = {0};
  esphome::openathan_component::NvsTimeFormatStore broken_store;
  TimeFormatPreferences broken(broken_store); broken.begin();
  CHECK(broken.hours() == 24 && !broken.writable());
  CHECK(broken.update(12,1) == TimeFormatResult::STORAGE);
  CHECK(nvs_test::committed.at({openathan_storage::PRAYER,"time_format"}) == std::vector<uint8_t>{0});
  for (const auto &[input, output] : {std::pair{"00:00","12:00 AM"}, {"00:59","12:59 AM"},
       {"01:05","1:05 AM"}, {"11:59","11:59 AM"}, {"12:00","12:00 PM"}, {"13:30","1:30 PM"}, {"23:59","11:59 PM"}})
    CHECK(std::string_view(format_clock(input,12).data()) == output);
  CHECK(std::string_view(format_clock("00:00",24).data()) == "00:00");
  for (auto input : {"24:00","12:60","1:00","ab:cd",""}) CHECK(!format_clock(input,12)[0]);
}
