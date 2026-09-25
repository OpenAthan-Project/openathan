#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace openathan {
enum class Method {
  MUSLIM_WORLD_LEAGUE, EGYPTIAN, KARACHI, UMM_AL_QURA, DUBAI, MOONSIGHTING_COMMITTEE,
  NORTH_AMERICA, KUWAIT, QATAR, SINGAPORE, TEHRAN, TURKEY
};
enum class HighLatitudeRule { MIDDLE_OF_NIGHT, SEVENTH_OF_NIGHT, TWILIGHT_ANGLE };
struct PrayerRequest {
  double latitude;
  double longitude;
  int year;
  unsigned month;
  unsigned day;
  Method method;
  bool hanafi{false};
  HighLatitudeRule high_latitude{HighLatitudeRule::MIDDLE_OF_NIGHT};
  std::array<int, 6> offsets{};
};
// Fajr, sunrise, Dhuhr, Asr, Maghrib, Isha; absent solar events remain absent.
// Input is the caller's local civil date; output is UTC Unix seconds.
using PrayerDay = std::array<std::optional<int64_t>, 6>;
bool calculate_prayers(const PrayerRequest &request, PrayerDay &result);
}  // namespace openathan
