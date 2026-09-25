#include <adhan/Astronomical.hpp>
#include <adhan/DateUtils.hpp>
#include <adhan/Madhab.hpp>
#include <adhan/PolarCircleResolution.hpp>
#include <adhan/PrayerTimes.hpp>
#include <adhan/SolarTime.hpp>
#include <adhan/TimeComponents.hpp>

#include <cmath>

namespace Adhan {

namespace {

/**
 * Less-than that reports false when either side is missing.
 *
 * Upstream gets this for free, since any comparison against an Invalid
 * Date is false. Comparing two std::optionals directly would instead
 * treat an empty one as the smaller value, which would quietly pick the
 * wrong branch below. Every ordering test in this file goes through here.
 */
bool before(const OptInstant &lhs, const OptInstant &rhs) {
  return lhs && rhs && *lhs < *rhs;
}

/**
 * Whether an instant has reached a prayer time. A missing prayer time
 * counts as not reached.
 */
bool atOrAfter(Instant when, const OptInstant &time) {
  return time && when >= *time;
}

} // namespace

PrayerTimes::PrayerTimes(
    const Coordinates &coordinates, const std::chrono::year_month_day &date,
    const CalculationParameters &calculationParameters)
    : coordinates(coordinates), date(date),
      calculationParameters(calculationParameters) {
  SolarTime solarTime(date, coordinates);

  OptInstant fajrTime;
  OptInstant sunriseTime;
  OptInstant dhuhrTime;
  OptInstant asrTime;
  OptInstant sunsetTime;
  OptInstant maghribTime;
  OptInstant ishaTime;

  double nightFraction = 0;

  dhuhrTime = TimeComponents(solarTime.transit).utcDate(date);
  sunriseTime = TimeComponents(solarTime.sunrise).utcDate(date);
  sunsetTime = TimeComponents(solarTime.sunset).utcDate(date);

  const auto tomorrow = dateByAddingDays(date, 1);
  SolarTime tomorrowSolarTime(tomorrow, coordinates);

  PolarCircleResolution polarCircleResolver =
      calculationParameters.polarCircleResolution;
  if ((!isValidDate(sunriseTime) || !isValidDate(sunsetTime) ||
       std::isnan(tomorrowSolarTime.sunrise)) &&
      polarCircleResolver != PolarCircleResolution::Unresolved) {
    PolarCircleResolver resolved =
        polarCircleResolvedValues(polarCircleResolver, date, coordinates);
    solarTime = resolved.solarTime;
    tomorrowSolarTime = resolved.tomorrowSolarTime;

    dhuhrTime = TimeComponents(solarTime.transit).utcDate(date);
    sunriseTime = TimeComponents(solarTime.sunrise).utcDate(date);
    sunsetTime = TimeComponents(solarTime.sunset).utcDate(date);
  }

  asrTime = TimeComponents(solarTime.afternoon(
                               shadow_length(calculationParameters.madhab)))
                .utcDate(date);

  const OptInstant tomorrowSunrise =
      TimeComponents(tomorrowSolarTime.sunrise).utcDate(tomorrow);
  const double night = secondsBetween(tomorrowSunrise, sunsetTime);

  fajrTime = TimeComponents(solarTime.hourAngle(
                                -1 * calculationParameters.fajrAngle, false))
                 .utcDate(date);

  // special case for moonsighting committee above latitude 55
  if (calculationParameters.method == "MoonsightingCommittee" &&
      coordinates.latitude >= 55) {
    nightFraction = night / 7;
    fajrTime = dateByAddingSeconds(sunriseTime, -nightFraction);
  }

  const OptInstant safeFajr = [&]() {
    if (calculationParameters.method == "MoonsightingCommittee") {
      return Astronomical::seasonAdjustedMorningTwilight(
          coordinates.latitude, dayOfYear(date),
          static_cast<int>(date.year()), sunriseTime);
    }
    double portion = calculationParameters.nightPortions().fajr;
    nightFraction = portion * night;
    return dateByAddingSeconds(sunriseTime, -nightFraction);
  }();

  if (!fajrTime || before(fajrTime, safeFajr)) {
    fajrTime = safeFajr;
  }

  if (calculationParameters.ishaInterval > 0) {
    ishaTime =
        dateByAddingMinutes(sunsetTime, calculationParameters.ishaInterval);
  } else {
    ishaTime =
        TimeComponents(
            solarTime.hourAngle(-1 * calculationParameters.ishaAngle, true))
            .utcDate(date);

    /* special case for moonsighting committee above latitude 55 */
    if (calculationParameters.method == "MoonsightingCommittee" &&
        coordinates.latitude >= 55) {
      nightFraction = night / 7;
      ishaTime = dateByAddingSeconds(sunsetTime, nightFraction);
    }

    const OptInstant safeIsha = [&]() {
      if (calculationParameters.method == "MoonsightingCommittee") {
        return Astronomical::seasonAdjustedEveningTwilight(
            coordinates.latitude, dayOfYear(date),
            static_cast<int>(date.year()), sunsetTime,
            calculationParameters.shafaq);
      }
      double portion = calculationParameters.nightPortions().isha;
      nightFraction = portion * night;
      return dateByAddingSeconds(sunsetTime, nightFraction);
    }();

    if (!ishaTime || before(safeIsha, ishaTime)) {
      ishaTime = safeIsha;
    }
  }

  maghribTime = sunsetTime;
  if (calculationParameters.maghribAngle != 0) {
    const OptInstant angleBasedMaghrib =
        TimeComponents(
            solarTime.hourAngle(-1 * calculationParameters.maghribAngle, true))
            .utcDate(date);
    if (before(sunsetTime, angleBasedMaghrib) &&
        before(angleBasedMaghrib, ishaTime)) {
      maghribTime = angleBasedMaghrib;
    }
  }

  int fajrAdjustment = calculationParameters.adjustments.fajr +
                       calculationParameters.methodAdjustments.fajr;
  int sunriseAdjustment = calculationParameters.adjustments.sunrise +
                          calculationParameters.methodAdjustments.sunrise;
  int dhuhrAdjustment = calculationParameters.adjustments.dhuhr +
                        calculationParameters.methodAdjustments.dhuhr;
  int asrAdjustment = calculationParameters.adjustments.asr +
                      calculationParameters.methodAdjustments.asr;
  int maghribAdjustment = calculationParameters.adjustments.maghrib +
                          calculationParameters.methodAdjustments.maghrib;
  int ishaAdjustment = calculationParameters.adjustments.isha +
                       calculationParameters.methodAdjustments.isha;

  fajr = roundedMinute(
      dateByAddingMinutes(fajrTime, fajrAdjustment),
      calculationParameters.rounding);
  sunrise = roundedMinute(
      dateByAddingMinutes(sunriseTime, sunriseAdjustment),
      calculationParameters.rounding);
  dhuhr = roundedMinute(
      dateByAddingMinutes(dhuhrTime, dhuhrAdjustment),
      calculationParameters.rounding);
  asr = roundedMinute(
      dateByAddingMinutes(asrTime, asrAdjustment),
      calculationParameters.rounding);
  sunset = roundedMinute(sunsetTime, calculationParameters.rounding);
  maghrib = roundedMinute(
      dateByAddingMinutes(maghribTime, maghribAdjustment),
      calculationParameters.rounding);
  isha = roundedMinute(
      dateByAddingMinutes(ishaTime, ishaAdjustment),
      calculationParameters.rounding);
}

OptInstant PrayerTimes::timeForPrayer(Prayer prayer) const {
  if (prayer == Prayer::Fajr) {
    return fajr;
  }
  if (prayer == Prayer::Sunrise) {
    return sunrise;
  }
  if (prayer == Prayer::Dhuhr) {
    return dhuhr;
  }
  if (prayer == Prayer::Asr) {
    return asr;
  }
  if (prayer == Prayer::Maghrib) {
    return maghrib;
  }
  if (prayer == Prayer::Isha) {
    return isha;
  }
  return std::nullopt;
}

Prayer PrayerTimes::currentPrayer(Instant when) const {
  if (atOrAfter(when, isha)) {
    return Prayer::Isha;
  }
  if (atOrAfter(when, maghrib)) {
    return Prayer::Maghrib;
  }
  if (atOrAfter(when, asr)) {
    return Prayer::Asr;
  }
  if (atOrAfter(when, dhuhr)) {
    return Prayer::Dhuhr;
  }
  if (atOrAfter(when, sunrise)) {
    return Prayer::Sunrise;
  }
  if (atOrAfter(when, fajr)) {
    return Prayer::Fajr;
  }
  return Prayer::None;
}

Prayer PrayerTimes::nextPrayer(Instant when) const {
  if (atOrAfter(when, isha)) {
    return Prayer::None;
  }
  if (atOrAfter(when, maghrib)) {
    return Prayer::Isha;
  }
  if (atOrAfter(when, asr)) {
    return Prayer::Maghrib;
  }
  if (atOrAfter(when, dhuhr)) {
    return Prayer::Asr;
  }
  if (atOrAfter(when, sunrise)) {
    return Prayer::Dhuhr;
  }
  if (atOrAfter(when, fajr)) {
    return Prayer::Sunrise;
  }
  return Prayer::Fajr;
}
} // namespace Adhan
