#include <adhan/Astronomical.hpp>
#include <adhan/MathUtils.hpp>
#include <adhan/SolarCoordinates.hpp>

#include <cmath>
#include <stdexcept>

namespace Adhan {

SolarCoordinates::SolarCoordinates(double julianDay) {
  if (!std::isfinite(julianDay)) {
    throw std::invalid_argument(
        "SolarCoordinates: julianDay must be a finite number");
  }
  const double T = Astronomical::julianCentury(julianDay);
  const double L0 = Astronomical::meanSolarLongitude(T);
  const double Lp = Astronomical::meanLunarLongitude(T);
  const double Omega = Astronomical::ascendingLunarNodeLongitude(T);
  const double Lambda =
      degreesToRadians(Astronomical::apparentSolarLongitude(T, L0));
  const double Theta0 = Astronomical::meanSiderealTime(T);
  const double dPsi = Astronomical::nutationInLongitude(T, L0, Lp, Omega);
  const double dEpsilon = Astronomical::nutationInObliquity(T, L0, Lp, Omega);
  const double Epsilon0 = Astronomical::meanObliquityOfTheEcliptic(T);
  const double EpsilonApparent = degreesToRadians(
      Astronomical::apparentObliquityOfTheEcliptic(T, Epsilon0));

  /**
   * declination: The declination of the sun, the angle between
   * the rays of the Sun and the plane of the Earth's equator, in degrees.
   * Equation from Astronomical Algorithms page 165
   */
  declination =
      radiansToDegrees(std::asin(std::sin(EpsilonApparent) * std::sin(Lambda)));

  /**
   * rightAscension: Right ascension of the Sun, the angular distance on the
   * celestial equator from the vernal equinox to the hour circle, in degrees.
   * Equation from Astronomical Algorithms page 165
   */
  rightAscension = unwindAngle(radiansToDegrees(
      std::atan2(
          std::cos(EpsilonApparent) * std::sin(Lambda), std::cos(Lambda))));

  /**
   * apparentSiderealTime: Apparent sidereal time, the hour angle of the vernal
   * equinox, in degrees. Equation from Astronomical Algorithms page 88
   */
  apparentSiderealTime =
      Theta0 +
      (dPsi * 3600 * std::cos(degreesToRadians(Epsilon0 + dEpsilon))) / 3600;
}
} // namespace Adhan