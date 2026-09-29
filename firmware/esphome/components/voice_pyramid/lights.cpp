#include "lights.h"
#ifdef USE_OPENATHAN_LIGHTS
namespace esphome::voice_pyramid {
bool Lights::apply(::openathan::LightFrame frame) {
  // M5Stack STM32Ctrl register protocol: 14 pixels/group, four-byte BGR0
  // records, global brightness 0..100. Do not call begin/resetSpeaker or save
  // brightness to STM32 flash. All transactions run on the ESPHome main loop.
  if (frame.brightness > 100) return false;
  const auto previous = applied_;
  applied_.reset(); // Any partial write forces a complete repair on retry.
  const bool color_changed = !previous || previous->red != frame.red ||
      previous->green != frame.green || previous->blue != frame.blue;
  if (color_changed) {
    const uint8_t dark = 0;
    if (!write_byte(0x10, dark) || !write_byte(0x11, dark)) return false;
    const uint8_t pixel[]{frame.blue, frame.green, frame.red, 0};
    for (uint8_t base : {0x20, 0x60})
      for (unsigned index = 0; index < 14; ++index)
        if (!write_bytes(base + index * 4, pixel, sizeof(pixel))) return false;
  }
  if (color_changed || previous->brightness != frame.brightness) {
    if (!write_byte(0x10, frame.brightness) || !write_byte(0x11, frame.brightness)) return false;
  }
  applied_ = frame;
  return true;
}
}  // namespace esphome::voice_pyramid
#endif
