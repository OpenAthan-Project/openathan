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
::openathan::LoadResult NvsLightStore::load(::openathan::SavedLights &value) {
  using ::openathan::LoadResult;
  if (!opened_) {
    if (nvs_open(openathan_storage::PRAYER, NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
    opened_ = true;
  }
  size_t length = 0;
  const auto result = nvs_get_blob(handle_, "lights", nullptr, &length);
  if (result == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  ::openathan::LightRecord record;
  if (result != ESP_OK || length != record.size() ||
      nvs_get_blob(handle_, "lights", record.data(), &length) != ESP_OK || length != record.size() ||
      !::openathan::decode_lights(record, value)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool NvsLightStore::save(const ::openathan::SavedLights &value) {
  if (!opened_ || !value.revision || value.value.brightness_percent > 100) return false;
  const auto record = ::openathan::encode_lights(value);
  return nvs_set_blob(handle_, "lights", record.data(), record.size()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}
::openathan::LoadResult NvsTimeFormatStore::load(::openathan::SavedTimeFormat &value) {
  using ::openathan::LoadResult;
  if (!opened_) {
    if (nvs_open(openathan_storage::PRAYER, NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
    opened_ = true;
  }
  size_t length = 0;
  const auto result = nvs_get_blob(handle_, "time_format", nullptr, &length);
  if (result == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  ::openathan::TimeFormatRecord record;
  if (result != ESP_OK || length != record.size() ||
      nvs_get_blob(handle_, "time_format", record.data(), &length) != ESP_OK || length != record.size() ||
      !::openathan::decode_time_format(record, value)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool NvsTimeFormatStore::save(const ::openathan::SavedTimeFormat &value) {
  if (!opened_ || !value.revision || (value.hours != 12 && value.hours != 24)) return false;
  const auto record = ::openathan::encode_time_format(value);
  return nvs_set_blob(handle_, "time_format", record.data(), record.size()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}
::openathan::LoadResult NvsDisplayStore::load(::openathan::SavedDisplay &value) {
  using ::openathan::LoadResult;
  if (!opened_) {
    if (nvs_open(openathan_storage::PRAYER, NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
    opened_ = true;
  }
  size_t length = 0;
  const auto result = nvs_get_blob(handle_, "display", nullptr, &length);
  if (result == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  ::openathan::DisplayRecord record;
  if (result != ESP_OK || length != record.size() ||
      nvs_get_blob(handle_, "display", record.data(), &length) != ESP_OK || length != record.size() ||
      !::openathan::decode_display(record, value)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool NvsDisplayStore::save(const ::openathan::SavedDisplay &value) {
  if (!opened_ || !value.revision || (value.brightness_percent < 1 || value.brightness_percent > 100)) return false;
  const auto record = ::openathan::encode_display(value);
  return nvs_set_blob(handle_, "display", record.data(), record.size()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}
}  // namespace esphome::openathan_component
