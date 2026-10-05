#include "openathan/display.h"
#include "nvs_settings_store.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
using namespace openathan;
namespace openathan_storage = esphome::openathan_storage;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
int main() {
  nvs_test::reset();
  esphome::openathan_component::NvsDisplayStore store;
  DisplayPreferences preferences(store); preferences.begin();
  CHECK(preferences.brightness_percent() == 10 && preferences.saved()->revision == 1 && nvs_test::writes == 0);
  CHECK(preferences.update(0, 1) == DisplayResult::INVALID);
  CHECK(preferences.update(12, 0) == DisplayResult::CONFLICT);
  CHECK(preferences.update(10, 1) == DisplayResult::UNCHANGED && nvs_test::writes == 0);
  nvs_test::committed[{openathan_storage::PRAYER, "settings"}] = {1,2,3};
  nvs_test::committed[{openathan_storage::PRAYER, "scheduler"}] = {4,5,6};
  const auto before = nvs_test::committed;
  CHECK(preferences.update(12, 1) == DisplayResult::SAVED);
  for (const auto &[key, value] : before) CHECK(nvs_test::committed.at(key) == value);
  CHECK(nvs_test::committed.contains({openathan_storage::PRAYER, "display"}));
  CHECK(!nvs_test::committed.contains({openathan_storage::TEST_MODE ? "openathan" : "oa_test", "display"}));
  nvs_test::power_cycle();
  esphome::openathan_component::NvsDisplayStore reboot_store;
  DisplayPreferences reboot(reboot_store); reboot.begin();
  CHECK(reboot.brightness_percent() == 12 && reboot.saved()->revision == 2);
  CHECK(reboot.update(10, 1) == DisplayResult::CONFLICT);
  nvs_test::fail_commit = true;
  CHECK(reboot.update(10, 2) == DisplayResult::STORAGE);
  CHECK(reboot.brightness_percent() == 12 && !reboot.writable());
  nvs_test::fail_commit = false; nvs_test::power_cycle();
  // Either permitted outcome of an interrupted write reloads a valid brightness.
  for (bool committed : {false, true}) for (bool early : {false, true}) {
    nvs_test::reset();
    esphome::openathan_component::NvsDisplayStore interrupted_store;
    DisplayPreferences interrupted(interrupted_store); interrupted.begin();
    nvs_test::early_persist=early; nvs_test::cut_after_write=!committed; nvs_test::cut_after_commit=committed;
    bool cut=false;
    try { interrupted.update(12,1); } catch (const std::runtime_error &) { cut=true; }
    CHECK(cut); nvs_test::power_cycle(); nvs_test::cut_after_write=nvs_test::cut_after_commit=false;
    esphome::openathan_component::NvsDisplayStore restored_store;
    DisplayPreferences restored(restored_store); restored.begin();
    CHECK(restored.writable() && restored.brightness_percent()==((committed || early) ? 12 : 10));
  }
  auto record = encode_display({2,12}); SavedDisplay decoded;
  CHECK(decode_display(record,decoded) && decoded.brightness_percent == 12);
  for (size_t i = 0; i < record.size(); ++i) {
    auto broken = record; broken[i] ^= 1; CHECK(!decode_display(broken,decoded));
  }
  CHECK(!decode_display(encode_display({2,101}),decoded));
  CHECK(!decode_display(encode_display({0,12}),decoded));
  nvs_test::committed[{openathan_storage::PRAYER,"display"}] = {0};
  esphome::openathan_component::NvsDisplayStore broken_store;
  DisplayPreferences broken(broken_store); broken.begin();
  CHECK(broken.brightness_percent() == 10 && !broken.writable());
  CHECK(broken.update(12,1) == DisplayResult::STORAGE);
  CHECK(nvs_test::committed.at({openathan_storage::PRAYER,"display"}) == std::vector<uint8_t>{0});
  CHECK(preferences.update(101, 1) == DisplayResult::INVALID);
  CHECK(!decode_display(encode_display({2,0}),decoded));
  nvs_test::reset();
  const auto full = encode_display({UINT32_MAX,100});
  nvs_test::committed[{openathan_storage::PRAYER,"display"}]={full.begin(),full.end()};
  esphome::openathan_component::NvsDisplayStore full_store;
  DisplayPreferences exhausted(full_store); exhausted.begin();
  CHECK(exhausted.update(1,UINT32_MAX)==DisplayResult::CONFLICT && nvs_test::writes==0);
  CHECK(exhausted.update(100,UINT32_MAX)==DisplayResult::UNCHANGED && nvs_test::writes==0);
  nvs_test::fail_read=true;
  esphome::openathan_component::NvsDisplayStore unreadable_store;
  DisplayPreferences unreadable(unreadable_store); unreadable.begin();
  CHECK(!unreadable.writable() && unreadable.brightness_percent()==10);

}
