// TODO: RECHECK THIS MODULE
#include "adhan/Astronomical.hpp"
#include "adhan/DateUtils.hpp"
#include "adhan/MathUtils.hpp"

#include <cmath>
#include <stdexcept>

namespace Adhan::Astronomical {

double meanSolarLongitude(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 163 */
  const double term1 = 280.4664567;
  const double term2 = 36000.76983 * T;
  const double term3 = 0.0003032 * std::pow(T, 2);
  const double L0 = term1 + term2 + term3;
  return unwindAngle(L0);
}

double meanLunarLongitude(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 144 */
  const double term1 = 218.3165;
  const double term2 = 481267.8813 * T;
  const double Lp = term1 + term2;
  return unwindAngle(Lp);
}

double ascendingLunarNodeLongitude(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 144 */
  const double term1 = 125.04452;
  const double term2 = 1934.136261 * T;
  const double term3 = 0.0020708 * std::pow(T, 2);
  const double term4 = std::pow(T, 3) / 450000;
  const double Omega = term1 - term2 + term3 + term4;
  return unwindAngle(Omega);
}

/* The mean anomaly of the sun. */
double meanSolarAnomaly(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 163 */
  const double term1 = 357.52911;
  const double term2 = 35999.05029 * T;
  const double term3 = 0.0001537 * std::pow(T, 2);
  const double M = term1 + term2 - term3;
  return unwindAngle(M);
}

double solarEquationOfTheCenter(double julianCentury, double meanAnomaly) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 164 */
  const double Mrad = degreesToRadians(meanAnomaly);
  const double term1 =
      (1.914602 - 0.004817 * T - 0.000014 * std::pow(T, 2)) * std::sin(Mrad);
  const double term2 = (0.019993 - 0.000101 * T) * std::sin(2 * Mrad);
  const double term3 = 0.000289 * std::sin(3 * Mrad);
  return term1 + term2 + term3;
}

double apparentSolarLongitude(double julianCentury, double meanLongitude) {
  const double T = julianCentury;
  const double L0 = meanLongitude;
  /* Equation from Astronomical Algorithms page 164 */
  const double longitude =
      L0 + solarEquationOfTheCenter(T, meanSolarAnomaly(T));
  const double Omega = 125.04 - 1934.136 * T;
  const double Lambda =
      longitude - 0.00569 - 0.00478 * std::sin(degreesToRadians(Omega));
  return unwindAngle(Lambda);
}

double meanObliquityOfTheEcliptic(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 147 */
  const double term1 = 23.439291;
  const double term2 = 0.013004167 * T;
  const double term3 = 0.0000001639 * std::pow(T, 2);
  const double term4 = 0.0000005036 * std::pow(T, 3);
  return term1 - term2 - term3 + term4;
}

double apparentObliquityOfTheEcliptic(
    double julianCentury, double meanObliquityOfTheEcliptic) {
  const double T = julianCentury;
  const double Epsilon0 = meanObliquityOfTheEcliptic;
  /* Equation from Astronomical Algorithms page 165 */
  const double O = 125.04 - 1934.136 * T;
  return Epsilon0 + 0.00256 * std::cos(degreesToRadians(O));
}

double meanSiderealTime(double julianCentury) {
  const double T = julianCentury;
  /* Equation from Astronomical Algorithms page 165 */
  const double JD = T * 36525 + 2451545.0;
  const double term1 = 280.46061837;
  const double term2 = 360.98564736629 * (JD - 2451545);
  const double term3 = 0.000387933 * std::pow(T, 2);
  const double term4 = std::pow(T, 3) / 38710000;
  const double Theta = term1 + term2 + term3 - term4;
  return unwindAngle(Theta);
}

double nutationInLongitude(
    double julianCentury, double solarLongitude, double lunarLongitude,
    double ascendingNode) {
  const double L0 = solarLongitude;
  const double Lp = lunarLongitude;
  const double Omega = ascendingNode;
  /* Equation from Astronomical Algorithms page 144 */
  const double term1 = (-17.2 / 3600) * std::sin(degreesToRadians(Omega));
  const double term2 = (1.32 / 3600) * std::sin(2 * degreesToRadians(L0));
  const double term3 = (0.23 / 3600) * std::sin(2 * degreesToRadians(Lp));
  const double term4 = (0.21 / 3600) * std::sin(2 * degreesToRadians(Omega));
  return term1 - term2 - term3 + term4;
}

double nutationInObliquity(
    double julianCentury, double solarLongitude, double lunarLongitude,
    double ascendingNode) {
  const double L0 = solarLongitude;
  const double Lp = lunarLongitude;
  const double Omega = ascendingNode;
  /* Equation from Astronomical Algorithms page 144 */
  const double term1 = (9.2 / 3600) * std::cos(degreesToRadians(Omega));
  const double term2 = (0.57 / 3600) * std::cos(2 * degreesToRadians(L0));
  const double term3 = (0.1 / 3600) * std::cos(2 * degreesToRadians(Lp));
  const double term4 = (0.09 / 3600) * std::cos(2 * degreesToRadians(Omega));
  return term1 + term2 + term3 - term4;
}

double altitudeOfCelestialBody(
    double observerLatitude, double declination, double localHourAngle) {
  const double Phi = observerLatitude;
  const double delta = declination;
  const double H = localHourAngle;
  /* Equation from Astronomical Algorithms page 93 */
  const double term1 =
      std::sin(degreesToRadians(Phi)) * std::sin(degreesToRadians(delta));
  const double term2 = std::cos(degreesToRadians(Phi)) *
                       std::cos(degreesToRadians(delta)) *
                       std::cos(degreesToRadians(H));
  return radiansToDegrees(std::asin(term1 + term2));
}

double approximateTransit(
    double longitude, double siderealTime, double rightAscension) {
  const double L = longitude;
  const double Theta0 = siderealTime;
  const double a2 = rightAscension;
  /* Equation from page Astronomical Algorithms 102 */
  const double Lw = L * -1;
  const double m0 = normalizeToScale((a2 + Lw - Theta0) / 360, 1);
  // For locations near the International Date Line, normalizeWithBound can
  // produce an m0 for the wrong calendar date.  We detect this by comparing m0
  // to a generalized transit time based on the longitude. If they differ by
  // more than half a day, m0 is off by one cycle and we adjust in the correct
  // direction.
  const double expectedTransit = normalizeToScale((12.0 - L / 15.0) / 24.0, 1);
  if (m0 - expectedTransit > 0.5) {
    return m0 - 1.0;
  }
  if (expectedTransit - m0 > 0.5) {
    return m0 + 1.0;
  }
  return m0;
}

double correctedTransit(
    double approximateTransit, double longitude, double siderealTime,
    double rightAscension, double previousRightAscension,
    double nextRightAscension) {
  const double m0 = approximateTransit;
  const double L = longitude;
  const double Theta0 = siderealTime;
  const double a2 = rightAscension;
  const double a1 = previousRightAscension;
  const double a3 = nextRightAscension;
  /* Equation from page Astronomical Algorithms 102 */
  const double Lw = L * -1;
  const double Theta = unwindAngle(Theta0 + 360.985647 * m0);
  const double a = unwindAngle(interpolateAngles(a2, a1, a3, m0));
  const double H = quadrantShiftAngle(Theta - Lw - a);
  const double dm = H / -360;
  return (m0 + dm) * 24;
}

double correctedHourAngle(
    double approximateTransit, double angle, const Coordinates &coordinates,
    bool afterTransit, double siderealTime, double rightAscension,
    double previousRightAscension, double nextRightAscension,
    double declination, double previousDeclination, double nextDeclination) {
  const double m0 = approximateTransit;
  const double h0 = angle;
  const double Theta0 = siderealTime;
  const double a2 = rightAscension;
  const double a1 = previousRightAscension;
  const double a3 = nextRightAscension;
  const double d2 = declination;
  const double d1 = previousDeclination;
  const double d3 = nextDeclination;

  /* Equation from page Astronomical Algorithms 102 */
  const double Lw = coordinates.longitude * -1;
  const double term1 = std::sin(degreesToRadians(h0)) -
                       std::sin(degreesToRadians(coordinates.latitude)) *
                           std::sin(degreesToRadians(d2));
  const double term2 = std::cos(degreesToRadians(coordinates.latitude)) *
                       std::cos(degreesToRadians(d2));
  const double H0 = radiansToDegrees(std::acos(term1 / term2));
  const double m = afterTransit ? m0 + H0 / 360 : m0 - H0 / 360;
  const double Theta = unwindAngle(Theta0 + 360.985647 * m);
  const double a = unwindAngle(interpolateAngles(a2, a1, a3, m));
  const double delta = interpolate(d2, d1, d3, m);
  const double H = Theta - Lw - a;
  const double h = altitudeOfCelestialBody(coordinates.latitude, delta, H);
  const double term3 = h - h0;
  const double term4 = 360 * std::cos(degreesToRadians(delta)) *
                       std::cos(degreesToRadians(coordinates.latitude)) *
                       std::sin(degreesToRadians(H));
  const double dm = term3 / term4;
  return (m + dm) * 24;
}

double interpolate(double y2, double y1, double y3, double n) {
  /* Equation from Astronomical Algorithms page 24 */
  const double a = y2 - y1;
  const double b = y3 - y2;
  const double c = b - a;
  return y2 + (n / 2) * (a + b + n * c);
}

double interpolateAngles(double y2, double y1, double y3, double n) {
  /* Equation from Astronomical Algorithms page 24 */
  const double a = unwindAngle(y2 - y1);
  const double b = unwindAngle(y3 - y2);
  const double c = b - a;
  return y2 + (n / 2) * (a + b + n * c);
}

double julianDay(int year, int month, int day, double hours) {
  /* Range validation */
  /* Month/Day is range-checked, but Hours are rolled over */
  if (month < 1 || month > 12) {
    throw std::invalid_argument(
        "Astronomical::julianDay: month must be in [1, 12]");
  }
  if (day < 1 || day > 31) {
    throw std::invalid_argument(
        "Astronomical::julianDay: day must be in [1, 31]");
  }

  /* Equation from Astronomical Algorithms page 60 */

  const double Y = std::trunc(month > 2 ? year : year - 1);
  const double M = std::trunc(month > 2 ? month : month + 12);
  const double D = day + hours / 24;

  const double A = std::trunc(Y / 100);
  const double B = std::trunc(2 - A + std::trunc(A / 4));

  const double i0 = std::trunc(365.25 * (Y + 4716));
  const double i1 = std::trunc(30.6001 * (M + 1));

  return i0 + i1 + D + B - 1524.5;
}

double julianCentury(double julianDay) {
  /* Equation from Astronomical Algorithms page 163 */
  return (julianDay - 2451545.0) / 36525;
}

OptInstant seasonAdjustedMorningTwilight(
    double latitude, int dayOfYear, int year, const OptInstant &sunrise) {
  const double a = 75 + (28.65 / 55.0) * std::abs(latitude);
  const double b = 75 + (19.44 / 55.0) * std::abs(latitude);
  const double c = 75 + (32.74 / 55.0) * std::abs(latitude);
  const double d = 75 + (48.1 / 55.0) * std::abs(latitude);

  double adjustment{};
  const int dyy = daysSinceSolstice(dayOfYear, year, latitude);
  if (dyy < 91) {
    adjustment = a + ((b - a) / 91.0) * dyy;
  } else if (dyy < 137) {
    adjustment = b + ((c - b) / 46.0) * (dyy - 91);
  } else if (dyy < 183) {
    adjustment = c + ((d - c) / 46.0) * (dyy - 137);
  } else if (dyy < 229) {
    adjustment = d + ((c - d) / 46.0) * (dyy - 183);
  } else if (dyy < 275) {
    adjustment = c + ((b - c) / 46.0) * (dyy - 229);
  } else {
    adjustment = b + ((a - b) / 91.0) * (dyy - 275);
  }

  return dateByAddingSeconds(sunrise, std::round(adjustment * -60.0));
}

OptInstant seasonAdjustedEveningTwilight(
    double latitude, int dayOfYear, int year, const OptInstant &sunset,
    Shafaq shafaq) {
  double a{};
  double b{};
  double c{};
  double d{};
  if (shafaq == Shafaq::Ahmer) {
    a = 62 + (17.4 / 55.0) * std::abs(latitude);
    b = 62 - (7.16 / 55.0) * std::abs(latitude);
    c = 62 + (5.12 / 55.0) * std::abs(latitude);
    d = 62 + (19.44 / 55.0) * std::abs(latitude);
  } else if (shafaq == Shafaq::Abyad) {
    a = 75 + (25.6 / 55.0) * std::abs(latitude);
    b = 75 + (7.16 / 55.0) * std::abs(latitude);
    c = 75 + (36.84 / 55.0) * std::abs(latitude);
    d = 75 + (81.84 / 55.0) * std::abs(latitude);
  } else {
    a = 75 + (25.6 / 55.0) * std::abs(latitude);
    b = 75 + (2.05 / 55.0) * std::abs(latitude);
    c = 75 - (9.21 / 55.0) * std::abs(latitude);
    d = 75 + (6.14 / 55.0) * std::abs(latitude);
  }

  double adjustment{};
  const int dyy = daysSinceSolstice(dayOfYear, year, latitude);
  if (dyy < 91) {
    adjustment = a + ((b - a) / 91.0) * dyy;
  } else if (dyy < 137) {
    adjustment = b + ((c - b) / 46.0) * (dyy - 91);
  } else if (dyy < 183) {
    adjustment = c + ((d - c) / 46.0) * (dyy - 137);
  } else if (dyy < 229) {
    adjustment = d + ((c - d) / 46.0) * (dyy - 183);
  } else if (dyy < 275) {
    adjustment = c + ((b - c) / 46.0) * (dyy - 229);
  } else {
    adjustment = b + ((a - b) / 91.0) * (dyy - 275);
  }

  return dateByAddingSeconds(sunset, std::round(adjustment * 60.0));
}

int daysSinceSolstice(int dayOfYear, int year, double latitude) {
  int daysSinceSolstice{};
  const int northernOffset = 10;
  const int southernOffset = isLeapYear(year) ? 173 : 172;
  const int daysInYear = isLeapYear(year) ? 366 : 365;

  if (latitude >= 0) {
    daysSinceSolstice = dayOfYear + northernOffset;
    if (daysSinceSolstice >= daysInYear) {
      daysSinceSolstice = daysSinceSolstice - daysInYear;
    }
  } else {
    daysSinceSolstice = dayOfYear - southernOffset;
    if (daysSinceSolstice < 0) {
      daysSinceSolstice = daysSinceSolstice + daysInYear;
    }
  }

  return daysSinceSolstice;
}

} // namespace Adhan::Astronomical
