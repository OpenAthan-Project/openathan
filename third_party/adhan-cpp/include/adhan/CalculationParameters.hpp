#ifndef CALCULATIONPARAMETERS_HPP
#define CALCULATIONPARAMETERS_HPP

#include "HighLatitudeRule.hpp"
#include "Madhab.hpp"
#include "PolarCircleResolution.hpp"
#include "Rounding.hpp"
#include "Shafaq.hpp"
#include <optional>
#include <string>

namespace Adhan {

using ManualAdjustments = struct ManualAdjustments {
  int fajr = 0;
  int sunrise = 0;
  int dhuhr = 0;
  int asr = 0;
  int maghrib = 0;
  int isha = 0;
};

struct NightPortions {
  double fajr;
  double isha;
};

class CalculationParameters {
public:
  /* Madhab to determine how Asr is calculated */
  Madhab madhab = Madhab::Shafi;

  /**
   * Rule to determine the earliest time for Fajr and latest time for Isha
   * needed for high latitude locations where Fajr and Isha may not truly exist
   * or may present a hardship unless bound to a reasonable time.
   */
  HighLatitudeRule highLatitudeRule = HighLatitudeRule::MiddleOfTheNight;

  /* Manual adjustments (in minutes) to be added to each prayer time. */
  ManualAdjustments adjustments;

  /**
   * Adjustments set by a calculation method.
   * This value should not be manually modified.
   */
  ManualAdjustments methodAdjustments;

  /**
   * Rule to determine how to resolve prayer times inside the Polar Circle
   * where daylight or night may persist for more than 24 hours depending
   * on the season
   */
  PolarCircleResolution polarCircleResolution =
      PolarCircleResolution::Unresolved;

  /* How seconds are rounded when calculating prayer times */
  Rounding rounding = Rounding::Nearest;

  /* Used by the MoonsightingCommittee method to determine how
   * to calculate Isha
   */
  Shafaq shafaq = Shafaq::General;

  /**
   * Name of the method, can be used to apply special behavior in calculations.
   * This property should not be manually modified.
   */
  std::string method;

  /* Angle of the sun below the horizon used for calculating Fajr. */
  double fajrAngle;

  /* Angle of the sun below the horizon used for calculating Isha. */
  double ishaAngle;

  /**
   * Minutes after Maghrib to determine time for Isha
   * if this value is greater than 0 then ishaAngle is not used.
   */
  double ishaInterval;

  /**
   * Angle of the sun below the horizon used for calculating Maghrib.
   * Only used by the Tehran method to account for lightness in the sky.
   */
  double maghribAngle;

  CalculationParameters(
      const std::optional<std::string> &method, double fajrAngle = 0,
      double ishaAngle = 0, double ishaInterval = 0, double maghribAngle = 0);

  NightPortions nightPortions() const;
};
} // namespace Adhan

#endif /* CALCULATIONPARAMETERS_HPP */