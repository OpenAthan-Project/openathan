#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/time/real_time_clock.h"
#include "openathan/scheduler.h"
#include "nvs_state_store.h"
#include "nvs_settings_store.h"
#include "setup_store.h"
#include "esphome/components/time/posix_tz.h"
#include "esphome/components/json/json_util.h"
#include <memory>

namespace esphome::openathan_component {
class OpenAthan : public PollingComponent, public ::openathan::Clock {
 public:
  OpenAthan() : PollingComponent(1000) {}
  void set_clock(time::RealTimeClock *clock) { clock_ = clock; }
  void set_playback(::openathan::Playback *playback) { playback_ = playback; }
  // Internal injection points, wired before setup by the developer test harness.
  void set_calculator(::openathan::DayCalculator *calculator) { calculator_source_ = calculator; }
  void set_state_store(::openathan::StateStore *store) { state_store_ = store; }
  void set_settings_store(::openathan::SettingsStore *store) { settings_store_ = store; }
  void set_default_timezone(const std::string &name) { timezone_name_ = name; }
  void require_setup() { setup_gate_ = std::make_unique<SetupStore>(); }
  const char *setup_state() const;
  bool activated() const { return !setup_gate_ || setup_gate_->active(); }
  bool finish_setup(uint32_t revision);
  bool preview(const ::openathan::DeviceSettings &settings, ::openathan::PrayerDay &day,
               std::vector<::openathan::Event> &events, std::vector<::openathan::ScheduleConflict> &conflicts);
  bool skip_occurrence(const ::openathan::Event &expected);
  bool cancel_occurrence(::openathan::EventKey expected);
  bool parse_settings_json(JsonObjectConst root, ::openathan::DeviceSettings &settings, uint32_t &revision) const;
  void reload_schedule();
  void set_latitude(double value) { settings_.latitude = value; }
  void set_longitude(double value) { settings_.longitude = value; }
  void set_method(::openathan::Method value) { settings_.method = value; }
  void set_hanafi(bool value) { settings_.hanafi = value; }
  void set_high_latitude(::openathan::HighLatitudeRule value) { settings_.high_latitude = value; }
  void set_offset(size_t index, int value) { if (index < 6) settings_.offsets[index] = value; }
  void set_enabled(size_t index, bool value) { if (index < 5) settings_.enabled[index] = value; }
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void update() override;
  ::openathan::ClockSample read() override;
  ::openathan::SchedulerStatus status() const;
  void stop();
  bool skip_next();
  bool cancel_skip();
  bool local_date(int64_t utc, const ::openathan::Timezone &timezone, ::openathan::CivilDate &date) const;
  std::string format_local(int64_t utc, const ::openathan::Timezone &timezone) const;
  bool local_date(int64_t utc, ::openathan::CivilDate &date) const;
  const ::openathan::SettingsService *settings_service() const { return settings_service_.get(); }
  ::openathan::SettingsResult change_settings(const ::openathan::DeviceSettings &candidate, uint32_t revision);
  void write_settings_json(JsonObject root) const;
  void read_settings_json(const std::string &payload);
  bool settings_request_ok() const { return request_ok_; }
  const std::string &settings_request_error() const { return request_error_; }
  const char *settings_application_status() const;
 private:
  void log_status_();
  void apply_volume_();
  time::RealTimeClock *clock_{};
  ::openathan::Playback *playback_{};
  ::openathan::Settings settings_;
  ::openathan::DayCalculator calculator_;
  NvsStateStore store_;
  NvsSettingsStore settings_nvs_;
  ::openathan::SettingsStore *settings_store_{&settings_nvs_};
  std::unique_ptr<::openathan::SettingsService> settings_service_;
  std::unique_ptr<SetupStore> setup_gate_;
  std::string timezone_name_;
  bool volume_applied_{false}, request_ok_{false};
  unsigned volume_attempts_{};
  uint64_t next_volume_check_{};
  std::string request_error_;
  ::openathan::DayCalculator *calculator_source_{&calculator_};
  ::openathan::StateStore *state_store_{&store_};
  std::unique_ptr<::openathan::Scheduler> scheduler_;
  std::string last_status_;
  std::string last_conflicts_;
};
template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  ControlAction(OpenAthan *parent, unsigned kind) : parent_(parent), kind_(kind) {}
  void play(const Ts &...args) override {
    if (kind_ == 0) parent_->stop();
    else if (kind_ == 1) parent_->skip_next();
    else parent_->cancel_skip();
  }
 private:
  OpenAthan *parent_;
  unsigned kind_;
};
}  // namespace esphome::openathan_component
