#pragma once
#include "esphome/components/openathan/openathan.h"

namespace esphome::openathan_validation {
// One immutable timetable per explicit start, retained through restart and OTA.
class Validation : public Component, public ::openathan::DayCalculator, public ::openathan::StateStore {
 public:
  void set_scheduler(openathan_component::OpenAthan *scheduler) { scheduler_ = scheduler; }
  float get_setup_priority() const override { return setup_priority::LATE + 1; }
  void setup() override;
  bool start_test();
  bool calculate(const ::openathan::Settings &, ::openathan::CivilDate, ::openathan::PrayerDay &) override;
  ::openathan::LoadResult load(::openathan::DurableState &) override;
  bool save(const ::openathan::DurableState &) override;
 private:
  openathan_component::OpenAthan *scheduler_{};
  openathan_component::NvsStateStore store_{"oa_validation"};
  openathan_component::NvsSettingsStore settings_store_{"oa_validation"};
  nvs_handle_t metadata_{};
  bool healthy_{false};
  int32_t day_{};
  int64_t first_{};
};
}  // namespace esphome::openathan_validation
