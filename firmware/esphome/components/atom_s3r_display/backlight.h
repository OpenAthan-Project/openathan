#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::atom_s3r_display {
class Backlight : public Component, public i2c::I2CDevice {
 public:
  void set_brightness(unsigned percent) { brightness_ = percent; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
 private:
  unsigned brightness_{10};
};
}
