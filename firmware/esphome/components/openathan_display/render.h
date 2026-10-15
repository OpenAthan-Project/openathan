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
template<typename Pixel> void draw_wrapped(Pixel &pixel, const char *value, int y, size_t columns, int width) {
  const std::string_view text(value);
  for (size_t start = 0; start < text.size(); start += columns, y += 12) {
    std::array<char, 33> line{};
    const auto length = std::min(columns, text.size() - start);
    std::copy_n(text.data() + start, length, line.data());
    draw_text(pixel, line.data(), y, 1, 0xFFFFFF, width);
  }
}
template<typename Pixel> void draw_qr(Pixel &pixel, const ConnectionFrame &connection, int x, int y, int scale) {
  // Version 3 is 29 modules. The white four-module quiet zone is part of the
  // painted square, not transparent space that a ring/text could intrude upon.
  constexpr int modules = 29, border = 4;
  for (int row = -border; row < modules + border; ++row)
    for (int col = -border; col < modules + border; ++col) {
      const uint32_t rgb = qrcodegen_getModule(connection.qr.data(), col, row) ? 0x000000 : 0xFFFFFF;
      for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx)
        pixel(x + (col + border)*scale + dx, y + (row + border)*scale + dy, rgb);
    }
}
template<typename Pixel> void render_connection(const Frame &frame, Pixel &pixel) {
  const auto &connection = frame.connection;
  if (frame.page == Page::QR && connection.connected && connection.qr_valid) {
    draw_qr(pixel, connection, 8, 8, 3);
    return;
  }
  draw_text(pixel, "Speaker page", 6, 1, 0xD0D8D8);
  draw_text(pixel, connection.connected ? "Same Wi-Fi" : "Offline", 24, 1, 0xD0D8D8);
  if (connection.available) draw_wrapped(pixel, connection.hostname.data(), 42, 16, 128);
  if (connection.connected && connection.ipv4[0]) draw_text(pixel, connection.ipv4.data(), 104, 1, 0xFFFFFF);
  else draw_text(pixel, "openathan.com", 104, 1, 0xFFFFFF);
  draw_text(pixel, connection.connected && connection.available ? "Press for clock" : "Use website USB", 116, 1, 0xD0D8D8);
}
inline const char *setup_heading(const ConnectionFrame &connection) {
  return connection.setup_error ? "Setup error" : connection.setup_connecting ? "Connecting..." : "Phone setup";
}
template<typename Pixel> void render_setup(const Frame &frame, Pixel &pixel) {
  const auto &connection = frame.connection;
  if (connection.setup_connecting) {
    draw_text(pixel, "Connecting...", 32, 1, 0xFFFFFF);
    draw_text(pixel, "Follow phone", 58, 1, 0xD0D8D8);
    draw_text(pixel, "instructions", 74, 1, 0xD0D8D8);
    draw_text(pixel, "Keep power on", 100, 1, 0xD0D8D8);
    return;
  }
  if (frame.page == Page::QR && connection.qr_valid) {
    draw_qr(pixel, connection, 8, 8, 3);
    return;
  }
  draw_text(pixel, setup_heading(connection), 6, 1, 0xFFFFFF);
  draw_text(pixel, connection.setup_error ? "Open setup page" : connection.setup_connecting ? "Please wait" : "Join Wi-Fi", 24, 1, 0xD0D8D8);
  draw_wrapped(pixel, connection.setup_ssid.data(), 42, 16, 128);
  draw_text(pixel, "Then open", 78, 1, 0xD0D8D8);
  draw_text(pixel, connection.ipv4[0] ? connection.ipv4.data() : "openathan.com", 94, 1, 0xFFFFFF);
  draw_text(pixel, connection.qr_valid ? "Press for QR" : "Use website USB", 116, 1, 0xD0D8D8);
}
template<typename Pixel> void render(const Frame &frame, Pixel pixel) {
  if (frame.connection.setup_active) { render_setup(frame, pixel); return; }
  if (frame.page != Page::STATUS) { render_connection(frame, pixel); return; }
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
// Every round frame has a native-resolution ring; unavailable timing is gray.
// Upcoming frames add a countdown. Other states retain the centered 2x layout.
template<typename Pixel> void render_round(const Frame &frame, Pixel pixel) {
  const uint32_t color = frame.proximity == LightMode::GREEN ? 0x00FF00 :
      frame.proximity == LightMode::ORANGE ? 0xFF6000 : frame.proximity == LightMode::RED ? 0xFF0000 : 0x606060;
  for (int y = 8; y < 352; ++y) for (int x = 8; x < 352; ++x) {
    const int dx = 2*x + 1 - 360, dy = 2*y + 1 - 360;
    const int distance = dx*dx + dy*dy;
    if (distance <= 344*344 && distance >= 328*328) pixel(x, y, color);
  }
  if (frame.connection.setup_active) {
    const auto &connection = frame.connection;
    if (connection.setup_connecting) {
      draw_text(pixel, "Connecting...", 112, 2, 0xFFFFFF, 360);
      draw_text(pixel, "Follow phone steps", 180, 2, 0xD0D8D8, 360);
      draw_text(pixel, "Keep power on", 248, 2, 0xD0D8D8, 360);
      return;
    }
    if (frame.page != Page::ADDRESS && connection.qr_valid && std::strlen(connection.setup_ssid.data()) <= 27) {
      draw_text(pixel, connection.setup_error ? "Setup error" : connection.setup_connecting ? "Connecting..." : "Join setup Wi-Fi", 48, 1, 0xD0D8D8, 360);
      draw_qr(pixel, connection, 69, 69, 6);
      draw_text(pixel, connection.setup_ssid.data(), 294, 1, 0xFFFFFF, 360);
      draw_text(pixel, connection.ipv4.data(), 310, 1, 0xD0D8D8, 360);
      draw_text(pixel, "Open if needed", 326, 1, 0xD0D8D8, 360);
    } else {
      draw_text(pixel, setup_heading(connection), 76, 2, 0xFFFFFF, 360);
      draw_text(pixel, connection.setup_error ? "Open setup page" : connection.setup_connecting ? "Please wait" : "Join Wi-Fi", 112, 2, 0xD0D8D8, 360);
      draw_wrapped(pixel, connection.setup_ssid.data(), 160, 32, 360);
      draw_text(pixel, "Then open", 208, 2, 0xD0D8D8, 360);
      draw_text(pixel, connection.ipv4[0] ? connection.ipv4.data() : "openathan.com", 248, 2, 0xFFFFFF, 360);
      draw_text(pixel, connection.qr_valid ? "Press for QR" : "Use website USB", 294, 1, 0xD0D8D8, 360);
    }
    return;
  }
  if (frame.page != Page::STATUS) {
    const auto &connection = frame.connection;
    if (connection.connected && connection.qr_valid && std::strlen(connection.hostname.data()) <= 27) {
      draw_text(pixel, "Open speaker page", 48, 1, 0xD0D8D8, 360);
      draw_qr(pixel, connection, 69, 69, 6);
      draw_text(pixel, connection.hostname.data(), 294, 1, 0xFFFFFF, 360);
      draw_text(pixel, connection.ipv4.data(), 310, 1, 0xD0D8D8, 360);
      draw_text(pixel, "Same Wi-Fi", 326, 1, 0xD0D8D8, 360);
    } else {
      draw_text(pixel, "Speaker page", 76, 2, 0xFFFFFF, 360);
      draw_text(pixel, connection.connected ? "Use address" : "Offline", 112, 2, 0xD0D8D8, 360);
      if (connection.available) draw_wrapped(pixel, connection.hostname.data(), 160, 32, 360);
      if (connection.connected) draw_text(pixel, connection.ipv4.data(), 224, 1, 0xFFFFFF, 360);
      draw_text(pixel, "openathan.com", 258, 2, 0xFFFFFF, 360);
      draw_text(pixel, "Use website USB", 294, 1, 0xD0D8D8, 360);
    }
    return;
  }
  if (frame.upcoming && frame.countdown[0]) {
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
    draw_text(pixel, frame.footer.data(), 310, std::strcmp(frame.footer.data(), "Press for page") == 0 ? 1 : 2, 0xD0D8D8, 360);
    return;
  }
  render(frame, [&pixel](int x, int y, uint32_t rgb) {
    for (int dy = 0; dy < 2; ++dy)
      for (int dx = 0; dx < 2; ++dx) pixel(52 + 2*x + dx, 52 + 2*y + dy, rgb);
  });
}
}  // namespace openathan::screen
