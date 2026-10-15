#pragma once
#include "esphome/core/component.h"
#include "esphome/components/display/display.h"
#include "esphome/components/openathan/openathan.h"
#include "render.h"
#include <functional>

namespace esphome::openathan_display {
class StatusDisplay : public PollingComponent {
 public:
  StatusDisplay() : PollingComponent(1000) {}
  void set_openathan(openathan_component::OpenAthan *value) { athan_ = value; }
  void set_display(display::Display *value) { display_ = value; }
  void set_round(bool value) { round_ = value; }
  void set_stop_button(bool value) { stop_button_ = value; }
  void set_connection_reader(std::function<::openathan::screen::ConnectionInfo()> reader) { connection_reader_ = std::move(reader); }
  void handle_control(unsigned action);
  ::openathan::screen::Page page() const { return page_; }
  float get_setup_priority() const override { return setup_priority::LATE - 10; }
  void setup() override;
  void update() override;
  void draw(display::Display &canvas) const;
 private:
  openathan_component::OpenAthan *athan_{};
  display::Display *display_{};
  ::openathan::screen::FrameCache cache_;
  ::openathan::screen::ConnectionCache connection_cache_;
  std::function<::openathan::screen::ConnectionInfo()> connection_reader_;
  ::openathan::screen::Page page_{::openathan::screen::Page::STATUS};
  uint32_t interaction_at_{};
  bool round_{};
  bool stop_button_{true};
};
}
