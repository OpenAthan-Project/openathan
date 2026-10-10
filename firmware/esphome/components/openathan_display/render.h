#pragma once
#include "presenter.h"
#include "font.h"
#include <cstring>

namespace openathan::screen {
// Shared by the actual LCD writer and the host preview; pixel colors are RGB.
template<typename Pixel> void draw_text_at(Pixel &pixel, const char *value, int x, int y, int scale, uint32_t color) {
  const auto length = std::strlen(value);
  for (size_t i = 0; i < length; ++i) {
    const unsigned ch = static_cast<unsigned char>(value[i]);
    const auto &glyph = FONT[(ch >= 32 && ch <= 126 ? ch : '?') - 32];
    for (int row = 0; row < 8; ++row)
      for (int col = 0; col < 8; ++col)
        if (glyph[row] & (1U << col))
          for (int dy = 0; dy < scale; ++dy)
            for (int dx = 0; dx < scale; ++dx)
              pixel(x + static_cast<int>(i) * 8 * scale + col * scale + dx,
                    y + row * scale + dy, color);
  }
}
template<typename Pixel> void draw_text(Pixel &pixel, const char *value, int y, int scale, uint32_t color, int width = 128) {
  const int x = (width - static_cast<int>(std::strlen(value)) * 8 * scale) / 2;
  draw_text_at(pixel, value, x, y, scale, color);
}
template<typename Pixel> void render(const Frame &frame, Pixel pixel) {
  draw_text(pixel, frame.clock.data(), 6, 1, 0xD0D8D8);
  if (frame.upcoming) {
    draw_text(pixel, frame.detail.data(), 30, 1, 0xD0D8D8);
    draw_text(pixel, frame.heading.data(), 44, 2, 0xFFFFFF);
    draw_text(pixel, frame.main.data(), 68, frame.main_scale, 0xFFFFFF);
    draw_text(pixel, frame.meridiem.data(), 100, 1, 0xD0D8D8);
  } else {
    draw_text(pixel, frame.heading.data(), 32, 2, 0xFFFFFF);
    draw_text(pixel, frame.main.data(), 58, frame.main_scale, 0xFFFFFF);
    draw_text(pixel, frame.meridiem.data(), 84, 1, 0xD0D8D8);
    draw_text(pixel, frame.detail.data(), 96, 1, frame.error ? 0xFFB8A8 : 0xD0D8D8);
  }
  draw_text(pixel, frame.footer.data(), 116, 1, 0xD0D8D8);
}
// Upcoming round frames add a native-resolution ring/countdown. Other states
// retain the 128-pixel layout centered in a 256-pixel square on the round panel.
template<typename Pixel> void render_round(const Frame &frame, Pixel pixel) {
  if (frame.upcoming && frame.countdown[0]) {
    const uint32_t color = frame.proximity == LightMode::GREEN ? 0x00FF00 :
        frame.proximity == LightMode::ORANGE ? 0xFF6000 : frame.proximity == LightMode::RED ? 0xFF0000 : 0;
    if (color) for (int y = 8; y < 352; ++y) for (int x = 8; x < 352; ++x) {
      const int dx = 2*x + 1 - 360, dy = 2*y + 1 - 360;
      const int distance = dx*dx + dy*dy;
      if (distance <= 344*344 && distance >= 328*328) pixel(x, y, color);
    }
    draw_text(pixel, frame.clock.data(), 64, 2, 0xD0D8D8, 360);
    draw_text(pixel, frame.detail.data(), 112, 2, 0xD0D8D8, 360);
    draw_text(pixel, frame.heading.data(), 140, 4, 0xFFFFFF, 360);
    const int time_width = static_cast<int>(std::strlen(frame.main.data())) * 8 * 6;
    const int suffix_width = static_cast<int>(std::strlen(frame.meridiem.data())) * 8 * 2;
    const int gap = suffix_width ? 12 : 0;
    const int x = (360 - time_width - gap - suffix_width) / 2;
    draw_text_at(pixel, frame.main.data(), x, 188, 6, 0xFFFFFF);
    // The uppercase/digit glyphs occupy rows 0–6; align their visible baselines.
    draw_text_at(pixel, frame.meridiem.data(), x + time_width + gap, 216, 2, 0xD0D8D8);
    draw_text(pixel, frame.countdown.data(), 252, 2, 0xD0D8D8, 360);
    draw_text(pixel, frame.footer.data(), 310, 2, 0xD0D8D8, 360);
    return;
  }
  render(frame, [&pixel](int x, int y, uint32_t rgb) {
    for (int dy = 0; dy < 2; ++dy)
      for (int dx = 0; dx < 2; ++dx) pixel(52 + 2*x + dx, 52 + 2*y + dy, rgb);
  });
}
}  // namespace openathan::screen
