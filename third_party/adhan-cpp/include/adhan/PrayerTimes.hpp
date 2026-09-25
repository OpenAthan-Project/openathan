#ifndef PRAYERTIMES_HPP
#define PRAYERTIMES_HPP

#include "CalculationParameters.hpp"
#include "Coordinates.hpp"
#include "DateUtils.hpp"
#include "Prayer.hpp"

#include <chrono>

namespace Adhan {

/**
 * @brief Prayer times for one calendar day at one place.
 *
 * The day going in is a plain calendar date with no zone attached, and the
 * times coming out are UTC instants. Deciding which calendar day a user is
 * currently in is the caller's job, since that is the only part that needs
 * a time zone.
 *
 * Any of the times can be absent. That happens where the sun does not rise
 * or set, or where the twilight angle is never reached.
 */
class PrayerTimes {
public:
  OptInstant fajr;
  OptInstant sunrise;
  OptInstant dhuhr;
  OptInstant asr;
  OptInstant sunset;
  OptInstant maghrib;
  OptInstant isha;

  Coordinates coordinates;
  std::chrono::year_month_day date;
  CalculationParameters calculationParameters;

  /**
   * @param coordinates Observer position.
   * @param date Calendar day to calculate.
   * @param calculationParameters Angles, madhab and the rest of the settings.
   */
  PrayerTimes(
      const Coordinates &coordinates, const std::chrono::year_month_day &date,
      const CalculationParameters &calculationParameters);

  /**
   * @brief Looks up one prayer by name.
   *
   * @return The time, or nullopt for Prayer::None and for a prayer whose
   *         time could not be worked out.
   */
  OptInstant timeForPrayer(Prayer prayer) const;

  /**
   * @brief The prayer whose time has most recently passed.
   *
   * @param when Instant to test, defaults to right now.
   * @return Prayer::None if the given instant is before Fajr.
   */
  Prayer currentPrayer(Instant when = now()) const;

  /**
   * @brief The prayer that comes next.
   *
   * @param when Instant to test, defaults to right now.
   * @return Prayer::None if Isha has already passed.
   */
  Prayer nextPrayer(Instant when = now()) const;
};
} // namespace Adhan

#endif // PRAYERTIMES_HPP
