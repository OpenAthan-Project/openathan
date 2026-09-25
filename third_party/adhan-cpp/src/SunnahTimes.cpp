#include <adhan/DateUtils.hpp>
#include <adhan/SunnahTimes.hpp>

namespace Adhan {

SunnahTimes::SunnahTimes(const PrayerTimes &prayerTimes) {
  const auto nextDay = dateByAddingDays(prayerTimes.date, 1);
  const auto nextDayPrayerTimes = PrayerTimes(
      prayerTimes.coordinates, nextDay, prayerTimes.calculationParameters);

  /**
   * A missing Maghrib or Fajr makes this NaN, which then travels through
   * dateByAddingSeconds and comes back out as an absent time.
   */
  const double nightDuration =
      secondsBetween(nextDayPrayerTimes.fajr, prayerTimes.maghrib);

  this->middleOfTheNight = roundedMinute(
      dateByAddingSeconds(prayerTimes.maghrib, nightDuration / 2.0));

  this->lastThirdOfTheNight = roundedMinute(
      dateByAddingSeconds(prayerTimes.maghrib, nightDuration * (2.0 / 3.0)));
}

} // namespace Adhan
