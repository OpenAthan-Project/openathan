#pragma once
#include "openathan/settings.h"
#include "openathan/lights.h"
#include "openathan/time_format.h"
#include "storage_config.h"
#include <nvs.h>

namespace esphome::openathan_component {
class NvsSettingsStore : public ::openathan::SettingsStore {
 public:
  explicit NvsSettingsStore(std::string name = openathan_storage::PRAYER) : namespace_(std::move(name)) {}
  NvsSettingsStore(const NvsSettingsStore &) = delete;
  NvsSettingsStore &operator=(const NvsSettingsStore &) = delete;
  ~NvsSettingsStore() override { if (opened_) nvs_close(handle_); }
  ::openathan::LoadResult load(::openathan::SavedSettings &settings) override;
  bool save(const ::openathan::SavedSettings &settings) override;
 private:
  std::string namespace_;
  nvs_handle_t handle_{};
  bool opened_{};
};
class NvsLightStore : public ::openathan::LightStore {
 public:
  ~NvsLightStore() override { if (opened_) nvs_close(handle_); }
  ::openathan::LoadResult load(::openathan::SavedLights &value) override;
  bool save(const ::openathan::SavedLights &value) override;
 private:
  nvs_handle_t handle_{};
  bool opened_{};
};
class NvsTimeFormatStore : public ::openathan::TimeFormatStore {
 public:
  ~NvsTimeFormatStore() override { if (opened_) nvs_close(handle_); }
  ::openathan::LoadResult load(::openathan::SavedTimeFormat &value) override;
  bool save(const ::openathan::SavedTimeFormat &value) override;
 private:
  nvs_handle_t handle_{};
  bool opened_{};
};
}  // namespace esphome::openathan_component
