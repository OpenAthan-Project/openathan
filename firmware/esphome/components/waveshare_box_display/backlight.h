#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/display/display.h"
#include "openathan/display.h"

namespace esphome::waveshare_box_display {
class Backlight : public Component, public ::openathan::DisplayOutput {
 public:
  void set_output(output::FloatOutput *value) { output_ = value; }
  void set_display(display::Display *value) { display_ = value; }
  void set_brightness(uint8_t value) { brightness_ = value; }
  float get_setup_priority() const override { return setup_priority::LATE + 10; }
  void setup() override { if (!apply(brightness_)) mark_failed(); }
  bool apply(uint8_t percent) override {
    if (!output_ || !display_ || display_->is_failed() || percent < 1 || percent > 100) return false;
    output_->set_level(percent / 100.0f);
    return true;
  }
 private:
  output::FloatOutput *output_{};
  display::Display *display_{};
  uint8_t brightness_{50};
};
}
