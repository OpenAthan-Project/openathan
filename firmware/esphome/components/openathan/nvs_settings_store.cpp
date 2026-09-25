#include "nvs_settings_store.h"

namespace esphome::openathan_component {
::openathan::LoadResult NvsSettingsStore::load(::openathan::SavedSettings &settings) {
  using ::openathan::LoadResult;
  if (!opened_) {
    if (nvs_open(namespace_.c_str(), NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
    opened_ = true;
  }
  size_t length = 0;
  const auto result = nvs_get_blob(handle_, "settings", nullptr, &length);
  if (result == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  ::openathan::SettingsRecord record;
  if (result != ESP_OK || length != record.size()) return LoadResult::ERROR;
  if (nvs_get_blob(handle_, "settings", record.data(), &length) != ESP_OK || length != record.size() ||
      !::openathan::decode_settings(record, settings)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool NvsSettingsStore::save(const ::openathan::SavedSettings &settings) {
  if (!opened_ || !settings.revision || !::openathan::valid_device_settings(settings.value)) return false;
  const auto record = ::openathan::encode_settings(settings);
  if (nvs_set_blob(handle_, "settings", record.data(), record.size()) != ESP_OK) return false;
  return nvs_commit(handle_) == ESP_OK;
}
}  // namespace esphome::openathan_component
