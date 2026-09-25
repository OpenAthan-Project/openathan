#include "setup_store.h"
#include "storage_config.h"

#include <array>

namespace esphome::openathan_component {
bool SetupStore::begin(::openathan::LoadResult settings, ::openathan::LoadResult history) {
  using ::openathan::LoadResult;
  healthy_ = active_ = false;
  if (settings == LoadResult::ERROR || history == LoadResult::ERROR ||
      (settings == LoadResult::EMPTY && history == LoadResult::LOADED))
    return false;
  if (nvs_open(openathan_storage::SETUP, NVS_READWRITE, &handle_) != ESP_OK) return false;
  opened_ = true;
  std::array<uint8_t, 8> record{};
  size_t length = record.size();
  const auto result = nvs_get_blob(handle_, "state", record.data(), &length);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    // Persist PENDING before SettingsService can create a defaults record.
    // A recognized pre-gate settings installation is adopted unchanged.
    return write_(settings == LoadResult::LOADED);
  }
  if (result != ESP_OK || length != record.size() || record[0] != 'O' || record[1] != 'A' || record[2] != 'S' ||
      record[3] != 1 || record[4] > 1 || record[5] != uint8_t(~record[4]) || record[6] != 0 || record[7] != 0 ||
      (record[4] && settings != LoadResult::LOADED))
    return false;
  active_ = record[4];
  healthy_ = true;
  return true;
}
bool SetupStore::write_(bool active) {
  const std::array<uint8_t, 8> record{'O', 'A', 'S', 1, uint8_t(active), uint8_t(~uint8_t(active)), 0, 0};
  if (!opened_ || nvs_set_blob(handle_, "state", record.data(), record.size()) != ESP_OK ||
      nvs_commit(handle_) != ESP_OK) {
    healthy_ = false;
    return false;
  }
  active_ = active;
  healthy_ = true;
  return true;
}
bool SetupStore::activate() { return healthy_ && (active_ || write_(true)); }
}  // namespace esphome::openathan_component
