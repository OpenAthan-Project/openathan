#include <adhan/Astronomical.hpp>
#include <adhan/MathUtils.hpp>
#include <adhan/SolarTime.hpp>

#include <cmath>

namespace Adhan {

namespace {

/**
 * year_month_day already counts months from 1, so nothing is shifted here.
 * The upstream code adds 1 because a JS Date counts them from 0.
 */
double julianDayOf(const std::chrono::year_month_day &date) {
  return Astronomical::julianDay(
      static_cast<int>(date.year()),
      static_cast<int>(static_cast<unsigned>(date.month())),
      static_cast<int>(static_cast<unsigned>(date.day())), 0);
}

} // namespace

SolarTime::SolarTime(
    const std::chrono::year_month_day &date, const Coordinates &coordinates)
    : observer(coordinates), solar(julianDayOf(date)),
      prevSolar(julianDayOf(date) - 1), nextSolar(julianDayOf(date) + 1) {
  const double m0 = Astronomical::approximateTransit(
      coordinates.longitude, solar.apparentSiderealTime, solar.rightAscension);
  const double solarAltitude = -50.0 / 60.0;

  approxTransit = m0;

  transit = Astronomical::correctedTransit(
      m0, coordinates.longitude, solar.apparentSiderealTime,
      solar.rightAscension, prevSolar.rightAscension, nextSolar.rightAscension);

  sunrise = Astronomical::correctedHourAngle(
      m0, solarAltitude, coordinates, false, solar.apparentSiderealTime,
      solar.rightAscension, prevSolar.rightAscension, nextSolar.rightAscension,
      solar.declination, prevSolar.declination, nextSolar.declination);

  sunset = Astronomical::correctedHourAngle(
      m0, solarAltitude, coordinates, true, solar.apparentSiderealTime,
      solar.rightAscension, prevSolar.rightAscension, nextSolar.rightAscension,
      solar.declination, prevSolar.declination, nextSolar.declination);
}

double SolarTime::hourAngle(double angle, bool afterTransit) const {
  return Astronomical::correctedHourAngle(
      approxTransit, angle, observer, afterTransit, solar.apparentSiderealTime,
      solar.rightAscension, prevSolar.rightAscension, nextSolar.rightAscension,
      solar.declination, prevSolar.declination, nextSolar.declination);
}

double SolarTime::afternoon(double shadowLength) const {
  // (UPSTREAM) TODO source shadow angle calculation
  const double tangent = std::abs(observer.latitude - solar.declination);
  const double inverse = shadowLength + std::tan(degreesToRadians(tangent));
  const double angle = radiansToDegrees(std::atan(1.0 / inverse));
  return hourAngle(angle, true);
}
} // namespace Adhan