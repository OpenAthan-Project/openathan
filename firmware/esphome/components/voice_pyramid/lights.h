#pragma once
#include "esphome/core/defines.h"
#ifdef USE_OPENATHAN_LIGHTS
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "openathan/lights.h"

namespace esphome::voice_pyramid {
class Lights : public Component, public i2c::I2CDevice, public ::openathan::LightOutput {
 public:
  void setup() override { apply({}); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  bool apply(::openathan::LightFrame frame) override;
 private:
  std::optional<::openathan::LightFrame> applied_;
};
}  // namespace esphome::voice_pyramid
#endif
