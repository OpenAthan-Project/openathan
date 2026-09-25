#include "voice_pyramid.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::voice_pyramid {
void Clock::setup() {
  // Verified A167 configuration: 27 MHz crystal -> PLLA 903.168 MHz,
  // fractional multiplier 33 + 169/375; CLK1 /80 -> 11.2896 MHz (44100 * 256).
  constexpr uint32_t p1 = 128 * 33 + 128 * 169 / 375 - 512;
  constexpr uint32_t p2 = 128 * 169 - 375 * (128 * 169 / 375);
  const uint8_t pll[] = {0x01, 0x77, uint8_t((p1 >> 16) & 3), uint8_t(p1 >> 8), uint8_t(p1),
                         uint8_t((p2 >> 16) & 15), uint8_t(p2 >> 8), uint8_t(p2)};
  // Integer multisynth: P1 = 128*80 - 512 = 9728, P2 = 0, P3 = 1.
  const uint8_t divider[] = {0, 1, 0, 0x26, 0, 0, 0, 0};
  bool ok = write_byte(3, 0xFF) && write_byte(16, 0x80) && write_byte(17, 0x80) &&
            write_byte(18, 0x80) && write_byte(183, 0xC0) && write_bytes(26, pll, sizeof(pll)) &&
            write_bytes(50, divider, sizeof(divider)) && write_byte(17, 0x4F) &&
            write_byte(177, 0xA0) && write_byte(3, 0xFD);
  uint8_t readback[8], control{}, enabled{};
  ok = ok && read_bytes(26, readback, 8) && std::memcmp(readback, pll, 8) == 0 &&
       read_bytes(50, readback, 8) && std::memcmp(readback, divider, 8) == 0 &&
       read_byte(17, &control) && control == 0x4F && read_byte(3, &enabled) && enabled == 0xFD;
  if (!ok) {
    ESP_LOGE("voice_pyramid", "44.1 kHz clock setup/readback failed");
    mark_failed();
    return;
  }
  ESP_LOGI("voice_pyramid", "44.1 kHz clock initialized");
}

void Amplifier::setup() {
  // Same reset and SYSCTRL values as the successful private audio diagnostic.
  set_timeout(1000, [this]() {
    if (!write_byte(0x00, 0xFF) || !write_byte(0x01, 0x78)) {
      ESP_LOGE("voice_pyramid", "AW87559 setup failed");
      mark_failed();
    }
  });
}
}  // namespace esphome::voice_pyramid
