#pragma once
#include "esphome/core/component.h"
#include "esphome/components/display/display.h"
#include "esphome/components/openathan/openathan.h"
#include "render.h"

namespace esphome::openathan_display {
class StatusDisplay : public PollingComponent {
 public:
  StatusDisplay() : PollingComponent(1000) {}
  void set_openathan(openathan_component::OpenAthan *value) { athan_ = value; }
  void set_display(display::Display *value) { display_ = value; }
  void set_round(bool value) { round_ = value; }
  float get_setup_priority() const override { return setup_priority::LATE - 10; }
  void setup() override;
  void update() override;
  void draw(display::Display &canvas) const;
 private:
  openathan_component::OpenAthan *athan_{};
  display::Display *display_{};
  ::openathan::screen::FrameCache cache_;
  bool round_{};
};
}
