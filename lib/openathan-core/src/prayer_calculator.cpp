#include "openathan/prayer_calculator.h"

#include <adhan/CalculationMethod.hpp>
#include <adhan/PrayerTimes.hpp>
#include <cmath>
#include <exception>

namespace openathan {
bool calculate_prayers(const PrayerRequest &r, PrayerDay &out) {
  out = {};
  if (!std::isfinite(r.latitude) || !std::isfinite(r.longitude) || r.latitude < -90 || r.latitude > 90 ||
      r.longitude < -180 || r.longitude > 180 || r.year < 1900 || r.year > 2100 || r.month < 1 ||
      r.month > 12 || r.day < 1 || r.day > 31)
    return false;
  const std::chrono::year_month_day date{std::chrono::year(r.year), std::chrono::month(r.month),
                                         std::chrono::day(r.day)};
  if (!date.ok())
    return false;
  using namespace Adhan::CalculationMethod;
  using Factory = Adhan::CalculationParameters (*)();
  static constexpr Factory methods[] = {MuslimWorldLeague, Egyptian, Karachi, UmmAlQura, Dubai,
      MoonsightingCommittee, NorthAmerica, Kuwait, Qatar, Singapore, Tehran, Turkey};
  const auto index = static_cast<unsigned>(r.method);
  if (index >= std::size(methods))
    return false;
  if (static_cast<unsigned>(r.high_latitude) > static_cast<unsigned>(HighLatitudeRule::AUTO))
    return false;
  for (int offset : r.offsets)
    if (offset < -120 || offset > 120)
      return false;
  try {
    auto parameters = methods[index]();
    parameters.madhab = r.hanafi ? Adhan::Madhab::Hanafi : Adhan::Madhab::Shafi;
    parameters.highLatitudeRule = r.high_latitude == HighLatitudeRule::AUTO
        ? Adhan::recommended(Adhan::Coordinates(r.latitude, r.longitude))
        : static_cast<Adhan::HighLatitudeRule>(r.high_latitude);
    parameters.adjustments = {r.offsets[0], r.offsets[1], r.offsets[2],
                              r.offsets[3], r.offsets[4], r.offsets[5]};
    const Adhan::PrayerTimes day(Adhan::Coordinates(r.latitude, r.longitude), date, parameters);
    const Adhan::OptInstant times[] = {day.fajr, day.sunrise, day.dhuhr, day.asr, day.maghrib, day.isha};
    for (size_t i = 0; i < out.size(); ++i)
      if (times[i])
        out[i] = std::chrono::duration_cast<std::chrono::seconds>(times[i]->time_since_epoch()).count();
    std::optional<int64_t> previous;
    for (const auto &instant : out) {
      if (!instant) continue;
      if (previous && *instant <= *previous) {
        out = {};
        return false;
      }
      previous = instant;
    }
    return true;
  } catch (const std::exception &) {
    out = {};
    return false;
  }
}
}  // namespace openathan
