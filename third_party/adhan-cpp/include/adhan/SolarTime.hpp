#ifndef SOLARTIME_HPP
#define SOLARTIME_HPP

#include "Coordinates.hpp"
#include "SolarCoordinates.hpp"

#include <chrono>

namespace Adhan {

class SolarTime {
public:
  Coordinates observer;
  SolarCoordinates solar;
  SolarCoordinates prevSolar;
  SolarCoordinates nextSolar;
  double approxTransit;
  double transit;
  double sunrise;
  double sunset;

  /**
   * @param date Calendar day the solar figures are worked out for.
   * @param coordinates Observer position.
   */
  SolarTime(
      const std::chrono::year_month_day &date,
      const Coordinates &coordinates);

  double hourAngle(double angle, bool afterTransit) const;
  double afternoon(double shadowLength) const;
};
} // namespace Adhan

#endif // SOLARTIME_HPP