#include <cstdio>
#include <cstdlib>

#include "credential_store.h"
#include "nvs_memory.h"
#include "nvs_settings_store.h"
#include "nvs_state_store.h"
#include "setup_store.h"
#include "test_maintenance.h"

#define CHECK(c)                                                   \
  do {                                                             \
    if (!(c)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
      std::abort();                                                \
    }                                                              \
  } while (0)
using namespace esphome;
using namespace esphome::openathan_component;
using namespace esphome::openathan_device;
using openathan::LoadResult;
const std::string hostname = "openathan-test-aabbcc.local";
const std::vector<std::string> allow{hostname, "oa_test", "oa_setup_test", "oa_network_test"};
void seed() {
  nvs_test::reset();
  for (const char* ns : {"openathan", "oa_setup", "oa_network", "oa_validation", "unrelated"})
    nvs_test::committed[{ns, "sentinel"}] = {1, 2, 3, 4};
}
auto protected_records() {
  auto records = nvs_test::committed;
  for (auto it = records.begin(); it != records.end();) {
    if (it->first.first == "oa_test" || it->first.first == "oa_setup_test" || it->first.first == "oa_network_test")
      it = records.erase(it);
    else
      ++it;
  }
  return records;
}
void fill_test_records() {
  SetupStore gate;
  CHECK(gate.begin(LoadResult::EMPTY, LoadResult::EMPTY));
  openathan::SavedSettings settings{1, {}};
  settings.value.timezone.name = "UTC";
  settings.value.volume = 70;
  NvsSettingsStore preferences;
  openathan::SavedSettings previous;
  CHECK(preferences.load(previous) == LoadResult::EMPTY);
  CHECK(preferences.save(settings));
  NvsStateStore history;
  openathan::DurableState previous_history;
  CHECK(history.load(previous_history) == LoadResult::EMPTY);
  openathan::DurableState state;
  state.consumed_through[0] = 20721;
  CHECK(history.save(state));
  CHECK(gate.activate());
  CredentialStore credentials;
  CHECK(credentials.save_wifi({"test network", "test password"}));
  CHECK(credentials.save_password({1, std::string(32, 'a')}));
}
int main() {
  seed();
  const auto baseline = protected_records();
  bool stopped = false;
  for (auto fields :
       {std::vector<std::string>{}, std::vector<std::string>{hostname, "openathan", "oa_setup", "oa_network"},
        std::vector<std::string>{hostname, "oa_test", "oa_setup_test", "oa_validation"}}) {
    CHECK(clear_test_storage(fields, hostname, [&] { stopped = true; }) == ClearResult::INVALID);
    CHECK(!stopped && nvs_test::writes == 0);
  }
  if (!openathan_storage::TEST_MODE) {
    CHECK(std::string(openathan_storage::PRAYER) == "openathan");
    CHECK(std::string(openathan_storage::SETUP) == "oa_setup");
    CHECK(std::string(openathan_storage::NETWORK) == "oa_network");
    CHECK(clear_test_storage(allow, hostname, [&] { stopped = true; }) == ClearResult::INVALID);
    CHECK(!stopped && nvs_test::writes == 0 && protected_records() == baseline);
    return 0;
  }
  CHECK(std::string(openathan_storage::PRAYER) == "oa_test");
  CHECK(std::string(openathan_storage::SETUP) == "oa_setup_test");
  CHECK(std::string(openathan_storage::NETWORK) == "oa_network_test");
  fill_test_records();
  CHECK(protected_records() == baseline);
  CHECK(nvs_test::committed.count({"oa_test", "settings"}) && nvs_test::committed.count({"oa_test", "scheduler"}));
  CHECK(nvs_test::committed.count({"oa_setup_test", "state"}) &&
        nvs_test::committed.count({"oa_network_test", "wifi"}));
  CHECK(nvs_test::committed.count({"oa_network_test", "password"}));
  CHECK(clear_test_storage(allow, hostname, [&] { stopped = true; }) == ClearResult::CLEARED);
  CHECK(stopped && nvs_test::committed == baseline);
  // Every cleanup write/commit boundary, both permitted NVS persistence timings.
  for (bool early : {false, true})
    for (bool commit : {false, true})
      for (unsigned cut = 1; cut <= 4; ++cut) {
        seed();
        fill_test_records();
        nvs_test::writes = nvs_test::commits = 0;
        nvs_test::early_persist = early;
        (commit ? nvs_test::cut_commit_number : nvs_test::cut_write_number) = cut;
        stopped = false;
        try {
          clear_test_storage(allow, hostname, [&] { stopped = true; });
          CHECK(false);
        } catch (const std::runtime_error&) {
        }
        CHECK(stopped && protected_records() == baseline);
        nvs_test::cut_write_number = nvs_test::cut_commit_number = 0;
        nvs_test::power_cycle();
        // After the pending marker can be durable, interruption never auto-activates.
        if (cut > 1 || commit || early) {
          SetupStore resumed;
          openathan::SavedSettings saved;
          openathan::DurableState history;
          NvsSettingsStore preferences;
          NvsStateStore state;
          CHECK(resumed.begin(preferences.load(saved), state.load(history)) && !resumed.active());
        }
        CHECK(clear_test_storage(allow, hostname, [] {}) == ClearResult::CLEARED);
        CHECK(nvs_test::committed == baseline);
      }
  for (unsigned failure = 0; failure < 3; ++failure) {
    seed();
    fill_test_records();
    if (failure == 0) nvs_test::fail_open = true;
    if (failure == 1) nvs_test::fail_write = true;
    if (failure == 2) nvs_test::fail_commit = true;
    stopped = false;
    CHECK(clear_test_storage(allow, hostname, [&] { stopped = true; }) == ClearResult::STORAGE);
    CHECK(stopped && protected_records() == baseline);
  }
}
