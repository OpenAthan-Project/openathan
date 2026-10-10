#pragma once
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::waveshare_box_rtc {

// PCF85063A, UTC, years 2000–2099, Sunday=0. Register 03h identifies
// OpenAthan-initialized time; vendor images can use a different year epoch.
class RTC : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void set_network_time(time::RealTimeClock *clock) { network_time_ = clock; }
  void setup() override;
  void update() override {}  // Restore once; never overwrite SNTP from RTC later.
  float get_setup_priority() const override { return setup_priority::DATA + 1; }
  const char *restore_result() const { return restore_result_; }
  const char *save_result() const { return save_result_; }

 protected:
  void restore_();
  void save_();
  time::RealTimeClock *network_time_{};
  const char *restore_result_{"pending"};
  const char *save_result_{"pending"};
};
}  // namespace esphome::waveshare_box_rtc
