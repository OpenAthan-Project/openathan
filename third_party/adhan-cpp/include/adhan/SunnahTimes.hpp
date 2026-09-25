#ifndef SUNNAHTIMES_HPP
#define SUNNAHTIMES_HPP

#include "DateUtils.hpp"
#include "PrayerTimes.hpp"

namespace Adhan {

/**
 * @brief Night markers worked out from Maghrib and the next day's Fajr.
 *
 * Both values are absent when either end of the night is missing.
 */
class SunnahTimes {
public:
  OptInstant middleOfTheNight;
  OptInstant lastThirdOfTheNight;

  /**
   * @param prayerTimes Times for the day the night starts on. The next day
   *        is calculated internally to find when the night ends.
   */
  SunnahTimes(const PrayerTimes &prayerTimes);
};
} // namespace Adhan

#endif /* SUNNAHTIMES_HPP */
