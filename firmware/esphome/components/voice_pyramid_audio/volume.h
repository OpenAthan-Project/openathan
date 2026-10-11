#pragma once
#include <algorithm>
#include <cmath>

namespace esphome::voice_pyramid_audio {
// A wider curve retains the old codec's quiet tail. ESPHome software attenuation
// spans -49..0 dB; below that range the codec supplies the remaining attenuation.
// The codec never exceeds register 0xBF (0 dB), and samples are never amplified.
inline float volume_db(unsigned percent) { return -96.5f * (1.0f - percent / 100.0f); }
inline unsigned legacy_register(unsigned percent) {
  return static_cast<unsigned>((percent / 100.0f) * 0.6f * 255.0f);
}
inline unsigned migrate_volume(unsigned percent) {
  return percent ? (legacy_register(percent) + 2) * 100 / 193 : 0;
}
inline unsigned rollback_volume(unsigned percent) {
  if (!percent) return 0;
  unsigned result = 0;
  for (unsigned old = 1; old <= 100; ++old) {
    if ((static_cast<int>(legacy_register(old)) - 191) * 0.5f <= volume_db(percent)) result = old;
  }
  return result;
}
struct VolumePlan {
  float codec_level{}, software_level{};
};
inline VolumePlan volume_plan(unsigned percent) {
  if (!percent) return {0.75f, 0.0f};
  const float db = volume_db(percent);
  const int reg = db >= -49.0f ? 191 : static_cast<int>(std::floor(191 + 2 * (db + 48.0f)));
  const float codec_db = (reg - 191) * 0.5f;
  // The quarter-register offset avoids float truncation into the preceding
  // register in ESPHome's linear 0..255 mapping; 191.25 / 255 is exactly 0.75.
  return {(reg + 0.25f) / 255.0f, std::clamp(1.0f + (db - codec_db) / 49.0f, 0.0f, 1.0f)};
}
}  // namespace esphome::voice_pyramid_audio
