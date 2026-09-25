#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/time/real_time_clock.h"
#include "openathan/scheduler.h"
#include "nvs_state_store.h"
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
  void update() override;
  ::openathan::ClockSample read() override;
  ::openathan::SchedulerStatus status() const;
  void stop();
  bool skip_next();
  bool cancel_skip();
 private:
  void log_status_();
  time::RealTimeClock *clock_{};
  ::openathan::Playback *playback_{};
  ::openathan::Settings settings_;
  ::openathan::DayCalculator calculator_;
  NvsStateStore store_;
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
