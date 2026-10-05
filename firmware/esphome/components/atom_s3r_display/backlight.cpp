// LP5562 initialization adapted from m5stack/esphome-yaml (MIT); see LICENSE and README.
#include "backlight.h"
#include "esphome/core/hal.h"

namespace esphome::atom_s3r_display {
void Backlight::setup() {
  if (brightness_ < 1 || brightness_ > 100) { mark_failed(); return; }
  const auto write = [this](uint8_t reg, uint8_t value) {
    if (!write_byte(reg, value)) { mark_failed(); return false; }
    return true;
  };
  if (!write(0x0D, 0xFF) || !write(0x00, 0x40)) return;  // Reset and enable LP5562, not the Pyramid.
  delayMicroseconds(500);
  if (!write(0x08, 0x01)) return;  // Internal clock, linear dimming; same as verified configuration.
  delayMicroseconds(200);
  // Direct PWM, no engines or RGB outputs. Keep the white-channel current at the verified 17.5 mA.
  if (!write(0x70, 0) || !write(0x05, 0) || !write(0x06, 0) || !write(0x07, 0) || !write(0x0F, 175)) return;
  if (!write(0x0E, static_cast<uint8_t>((brightness_ * 255 + 50) / 100))) return;
  applied_ = brightness_;
  initialized_ = true;
}
bool Backlight::apply(uint8_t percent) {
  if (!initialized_ || is_failed() || percent < 1 || percent > 100) return false;
  if (percent == applied_) return true;
  // Runtime changes touch only PWM, never reset, channel current or RGB outputs.
  // A failed acknowledgement may still have changed PWM; invalidate the cache
  // so restoring the previous preference performs a real write too.
  applied_ = 0;
  if (!write_byte(0x0E, static_cast<uint8_t>((percent * 255 + 50) / 100))) return false;
  applied_ = percent;
  return true;
}
}
