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
  bool ready() const { return ready_ && !is_failed(); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
 private:
  bool ready_{};
};
}  // namespace esphome::voice_pyramid
