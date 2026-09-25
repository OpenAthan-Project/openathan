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
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
};
}  // namespace esphome::voice_pyramid
