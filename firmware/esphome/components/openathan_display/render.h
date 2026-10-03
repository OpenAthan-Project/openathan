#pragma once
#include "presenter.h"
#include "font.h"
#include <cstring>

namespace openathan::screen {
// Shared by the actual LCD writer and the host preview; pixel colors are RGB.
template<typename Pixel> void draw_text(Pixel &pixel, const char *value, int y, int scale, uint32_t color) {
  const auto length = std::strlen(value);
  const int x = (128 - static_cast<int>(length) * 8 * scale) / 2;
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
template<typename Pixel> void render(const Frame &frame, Pixel pixel) {
  draw_text(pixel, frame.clock.data(), 6, 1, 0xD0D8D8);
  draw_text(pixel, frame.heading.data(), 32, 2, 0xFFFFFF);
  draw_text(pixel, frame.main.data(), 58, frame.main_scale, 0xFFFFFF);
  draw_text(pixel, frame.detail.data(), 96, 1, frame.error ? 0xFFB8A8 : 0xD0D8D8);
  draw_text(pixel, frame.footer.data(), 116, 1, 0xD0D8D8);
}
}  // namespace openathan::screen
