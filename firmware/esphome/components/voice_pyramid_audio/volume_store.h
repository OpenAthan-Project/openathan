#pragma once
#include "esphome/components/openathan/nvs_settings_store.h"
#include "volume.h"
#include <limits>

namespace esphome::voice_pyramid_audio {
// Keep the v1 settings key readable by older firmware. A CRC-protected companion
// holds the new volume only while its complete legacy mirror still matches.
class VolumeSettingsStore : public ::openathan::SettingsStore {
 public:
  explicit VolumeSettingsStore(std::string name = openathan_storage::PRAYER) :
      namespace_(std::move(name)), legacy_(namespace_) {}
  ~VolumeSettingsStore() override { if (opened_) nvs_close(handle_); }
  ::openathan::LoadResult load(::openathan::SavedSettings &settings) override {
    using ::openathan::LoadResult;
    ::openathan::SavedSettings legacy;
    const auto result = legacy_.load(legacy);
    if (result == LoadResult::ERROR) return result;
    if (!opened_) {
      if (nvs_open(namespace_.c_str(), NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
      opened_ = true;
    }
    size_t length = 0;
    const auto found = nvs_get_blob(handle_, KEY, nullptr, &length);
    if (found != ESP_OK && found != ESP_ERR_NVS_NOT_FOUND) return LoadResult::ERROR;
    if (result == LoadResult::EMPTY) return found == ESP_ERR_NVS_NOT_FOUND ? result : LoadResult::ERROR;
    if (found == ESP_OK) {
      ::openathan::SettingsRecord record;
      ::openathan::SavedSettings current;
      if (length != record.size() || nvs_get_blob(handle_, KEY, record.data(), &length) != ESP_OK ||
          length != record.size() || !::openathan::decode_settings(record, current)) return LoadResult::ERROR;
      auto mirror = current;
      mirror.value.volume = rollback_volume(current.value.volume);
      if (mirror == legacy) { settings = std::move(current); return LoadResult::LOADED; }
      // Older firmware saved a new revision. Its volume is authoritative again.
    }
    if (legacy.revision == std::numeric_limits<uint32_t>::max()) return LoadResult::ERROR;
    settings = std::move(legacy);
    settings.value.volume = migrate_volume(settings.value.volume);
    ++settings.revision;  // Invalidate an old browser's settings revision.
    return LoadResult::LOADED;  // Boot does not rewrite flash or consumption.
  }
  bool save(const ::openathan::SavedSettings &settings) override {
    if (!opened_ || !settings.revision || !::openathan::valid_device_settings(settings.value)) return false;
    auto mirror = settings;
    mirror.value.volume = rollback_volume(settings.value.volume);
    // Commit the quiet, backwards-readable mirror first. An interrupted second
    // write either matches a complete companion or loads the quieter mirror.
    if (!legacy_.save(mirror)) return false;
    const auto record = ::openathan::encode_settings(settings);
    return nvs_set_blob(handle_, KEY, record.data(), record.size()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
  }
 private:
  static constexpr const char *KEY = "pyr_volume";
  std::string namespace_;
  openathan_component::NvsSettingsStore legacy_;
  nvs_handle_t handle_{};
  bool opened_{};
};
}  // namespace esphome::voice_pyramid_audio
