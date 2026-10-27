#pragma once
#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "openathan/lights.h"

namespace esphome::waveshare_audio_board {
class Ring : public Component, public ::openathan::LightOutput {
 public:
  void set_light(light::LightState *value) { light_ = value; }
  void set_driver(Component *value) { driver_ = value; }
  bool apply(::openathan::LightFrame frame) override {
    if (!light_ || !driver_ || driver_->is_failed()) return false;
    auto call = light_->make_call();
    call.set_state(frame.brightness != 0);
    call.set_transition_length(0);
    call.set_rgb(frame.red / 255.0f, frame.green / 255.0f, frame.blue / 255.0f);
    call.set_brightness(frame.brightness / 100.0f);
    call.perform();
    return true;  // Requested output, not physical light-emission confirmation.
  }
 private:
  light::LightState *light_{};
  Component *driver_{};
};
}
