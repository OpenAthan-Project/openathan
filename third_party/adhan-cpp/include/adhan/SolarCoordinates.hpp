#ifndef SOLARCOORDINATES_HPP
#define SOLARCOORDINATES_HPP

namespace Adhan {

class SolarCoordinates {
public:
  double declination;
  double rightAscension;
  double apparentSiderealTime;

  explicit SolarCoordinates(double julianDay);
};
} // namespace Adhan

#endif // SOLARCOORDINATES_HPP