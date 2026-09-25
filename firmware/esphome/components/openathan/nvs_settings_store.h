#pragma once
#include "openathan/settings.h"
#include <nvs.h>

namespace esphome::openathan_component {
class NvsSettingsStore : public ::openathan::SettingsStore {
 public:
  explicit NvsSettingsStore(std::string name = "openathan") : namespace_(std::move(name)) {}
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
}  // namespace esphome::openathan_component
