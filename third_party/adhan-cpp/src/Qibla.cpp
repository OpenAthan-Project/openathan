#include <adhan/Coordinates.hpp>
#include <adhan/MathUtils.hpp>
#include <adhan/Qibla.hpp>

namespace Adhan {

const Coordinates &makkah() {
  static const Coordinates instance{21.4225241, 39.8261818};
  return instance;
}

double qibla(const Coordinates &coordinates) {
  /**
   *  The following Equation is from:
   *  "Spherical Trigonometry For the use of colleges and schools", page 50
   */

  auto term1 = std::sin(
      degreesToRadians(makkah().longitude) -
      degreesToRadians(coordinates.longitude));

  auto term2 = std::cos(degreesToRadians(coordinates.latitude)) *
               std::tan(degreesToRadians(makkah().latitude));

  auto term3 = std::sin(degreesToRadians(coordinates.latitude)) *
               std::cos(
                   degreesToRadians(makkah().longitude) -
                   degreesToRadians(coordinates.longitude));

  auto angle = std::atan2(term1, term2 - term3);

  return unwindAngle(radiansToDegrees(angle));
}

} // namespace Adhan