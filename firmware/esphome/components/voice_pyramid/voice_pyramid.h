#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::voice_pyramid {
class Clock : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::IO; }
};
class Amplifier : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void set_enable_on_startup(bool enabled) { enable_on_startup_ = enabled; }
  bool set_pa_enabled(bool enabled);
  void on_shutdown() override { set_pa_enabled(false); }
  bool ready() const { return ready_ && !is_failed(); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
 private:
  bool ready_{};
  bool enable_on_startup_{true};
};
}  // namespace esphome::voice_pyramid
