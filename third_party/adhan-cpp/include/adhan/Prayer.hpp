#ifndef PRAYER_HPP
#define PRAYER_HPP

#include <cstdint>
#include <string_view>

namespace Adhan {

enum class Prayer : std::int8_t {
  Fajr,
  Sunrise,
  Dhuhr,
  Asr,
  Maghrib,
  Isha,
  None,
};

namespace PrayerUtils {

constexpr std::string_view to_string(Prayer prayer) {
  switch (prayer) {
  case Prayer::Fajr:
    return "fajr";
  case Prayer::Sunrise:
    return "sunrise";
  case Prayer::Dhuhr:
    return "dhuhr";
  case Prayer::Asr:
    return "asr";
  case Prayer::Maghrib:
    return "maghrib";
  case Prayer::Isha:
    return "isha";
  case Prayer::None:
  default:
    return "none";
  }
}

constexpr Prayer from_string(std::string_view value) {
  if (value == "fajr") {
    return Prayer::Fajr;
  }
  if (value == "sunrise") {
    return Prayer::Sunrise;
  }
  if (value == "dhuhr") {
    return Prayer::Dhuhr;
  }
  if (value == "asr") {
    return Prayer::Asr;
  }
  if (value == "maghrib") {
    return Prayer::Maghrib;
  }
  if (value == "isha") {
    return Prayer::Isha;
  }

  /* Defaults to "none" */
  return Prayer::None;
}

} // namespace PrayerUtils
} // namespace Adhan

#endif /* PRAYER_HPP */