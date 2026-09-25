#include "test_maintenance.h"

#include "../openathan/storage_config.h"
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
#include <nvs.h>

#include <array>
#endif

namespace esphome::openathan_device {
ClearResult clear_test_storage(const std::vector<std::string>& fields, const std::string& hostname,
                               const std::function<void()>& quiesce) {
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
  const std::vector<std::string> expected{hostname, "oa_test", "oa_setup_test", "oa_network_test"};
  if (hostname.rfind("openathan-test-", 0) != 0 || fields != expected) return ClearResult::INVALID;
  quiesce();  // Latches all application writes off until a physical restart.
  nvs_handle_t handle;
  if (nvs_open(openathan_storage::SETUP, NVS_READWRITE, &handle) != ESP_OK) return ClearResult::STORAGE;
  // Persist PENDING first. A cut while deleting other records cannot adopt
  // leftover test settings as an activated legacy installation.
  const std::array<uint8_t, 8> pending{'O', 'A', 'S', 1, 0, 255, 0, 0};
  bool ok = nvs_set_blob(handle, "state", pending.data(), pending.size()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  if (!ok) return ClearResult::STORAGE;
  // SETUP is last: absence must imply the other test groups were cleared.
  for (const char* name : {openathan_storage::PRAYER, openathan_storage::NETWORK, openathan_storage::SETUP}) {
    if (nvs_open(name, NVS_READWRITE, &handle) != ESP_OK) return ClearResult::STORAGE;
    ok = nvs_erase_all(handle) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    if (!ok) return ClearResult::STORAGE;
  }
  return ClearResult::CLEARED;
#else
  return ClearResult::INVALID;
#endif
}
}  // namespace esphome::openathan_device
