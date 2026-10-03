#pragma once
#include "esphome/core/component.h"
#include <cstdint>
#include <functional>

namespace esphome {
struct Color {
  uint8_t red{}, green{}, blue{};
  Color() = default;
  Color(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
  static const Color BLACK;
};
inline const Color Color::BLACK{};
namespace display {
class Display : public PollingComponent {
 public:
  Display() : PollingComponent(1000) {}
  virtual int get_width() { return 128; }
  virtual int get_height() { return 128; }
  virtual void draw_pixel_at(int, int, Color) {}
  virtual void fill(Color) {}
  void set_writer(std::function<void(Display &)> writer) { writer_ = writer; }
  void update() override { ++updates; if (writer_) writer_(*this); }
  unsigned updates{};
 private:
  std::function<void(Display &)> writer_;
};
}
}
