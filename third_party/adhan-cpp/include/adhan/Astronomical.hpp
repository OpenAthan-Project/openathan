#ifndef ASTRONOMICAL_HPP
#define ASTRONOMICAL_HPP

#include "Coordinates.hpp"
#include "DateUtils.hpp"
#include "Shafaq.hpp"

namespace Adhan {

namespace Astronomical {

/* The geometric mean longitude of the sun in degrees. */
double meanSolarLongitude(double julianCentury);

/* The geometric mean longitude of the moon in degrees. */
double meanLunarLongitude(double julianCentury);

double ascendingLunarNodeLongitude(double julianCentury);

/* The mean anomaly of the sun. */
double meanSolarAnomaly(double julianCentury);

/* The Sun's equation of the center in degrees. */
double solarEquationOfTheCenter(double julianCentury, double meanAnomaly);

/**
 * The apparent longitude of the Sun, referred to the
 * true equinox of the date.
 */
double apparentSolarLongitude(double julianCentury, double meanLongitude);

/**
 * The mean obliquity of the ecliptic, formula
 * adopted by the International Astronomical Union.
 * Represented in degrees.
 */
double meanObliquityOfTheEcliptic(double julianCentury);

/**
 * The mean obliquity of the ecliptic, corrected for
 * calculating the apparent position of the sun, in degrees.
 */
double apparentObliquityOfTheEcliptic(double julianCentury,
                                      double meanObliquityOfTheEcliptic);

/* Mean sidereal time, the hour angle of the vernal equinox, in degrees. */
double meanSiderealTime(double julianCentury);

double nutationInLongitude(double julianCentury, double solarLongitude,
                           double lunarLongitude, double ascendingNode);

double nutationInObliquity(double julianCentury, double solarLongitude,
                           double lunarLongitude, double ascendingNode);

double altitudeOfCelestialBody(double observerLatitude, double declination,
                               double localHourAngle);

double approximateTransit(double longitude, double siderealTime,
                          double rightAscension);

/**
 *  The time at which the sun is at its highest point in the sky
 * (in universal time)
 */
double correctedTransit(double approximateTransit, double longitude,
                        double siderealTime, double rightAscension,
                        double previousRightAscension,
                        double nextRightAscension);

double correctedHourAngle(double approximateTransit, double angle,
                          const Coordinates &coordinates, bool afterTransit,
                          double siderealTime, double rightAscension,
                          double previousRightAscension,
                          double nextRightAscension, double declination,
                          double previousDeclination, double nextDeclination);

/**
 * Interpolation of a value given equidistant
 * previous and next values and a factor
 * equal to the fraction of the interpolated
 * point's time over the time between values.
 */
double interpolate(double y2, double y1, double y3, double n);

/* Interpolation of three angles, accounting for angle unwinding. */
double interpolateAngles(double y2, double y1, double y3, double n);

/* The Julian Day for the given Gregorian date components. */
double julianDay(int year, int month, int day, double hours = 0);

/* Julian century from the epoch. */
double julianCentury(double julianDay);

/**
 * @brief Fajr for the MoonsightingCommittee method, backed off from sunrise
 *        by an amount that shifts with the season.
 *
 * @param latitude Observer latitude in degrees.
 * @param dayOfYear Position of the date within its year.
 * @param year Gregorian year, needed to spot a leap year.
 * @param sunrise Sunrise for the same day, may be absent.
 * @return The adjusted time, or nullopt if sunrise was absent.
 */
OptInstant seasonAdjustedMorningTwilight(double latitude, int dayOfYear,
                                         int year, const OptInstant &sunrise);

/**
 * @brief Isha for the MoonsightingCommittee method, pushed out from sunset
 *        by an amount that shifts with the season and the chosen shafaq.
 *
 * @param latitude Observer latitude in degrees.
 * @param dayOfYear Position of the date within its year.
 * @param year Gregorian year, needed to spot a leap year.
 * @param sunset Sunset for the same day, may be absent.
 * @param shafaq Which twilight the method should track.
 * @return The adjusted time, or nullopt if sunset was absent.
 */
OptInstant seasonAdjustedEveningTwilight(double latitude, int dayOfYear,
                                         int year, const OptInstant &sunset,
                                         Shafaq shafaq);

int daysSinceSolstice(int dayOfYear, int year, double latitude);

} // namespace Astronomical
} // namespace Adhan

#endif // ASTRONOMICAL_HPP