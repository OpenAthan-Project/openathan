#ifndef MATHUTILS_HPP
#define MATHUTILS_HPP

#include <cmath>
#include <numbers>

namespace Adhan {

inline constexpr double PI = std::numbers::pi;

constexpr double degreesToRadians(double degrees) {
  return (degrees * PI) / 180.0;
}

constexpr double radiansToDegrees(double radians) {
  return (radians * 180.0) / PI;
}

inline double normalizeToScale(double num, double max) {
  return num - max * std::floor(num / max);
}

inline double unwindAngle(double angle) {
  return normalizeToScale(angle, 360.0);
}

inline double quadrantShiftAngle(double angle) {
  if (angle >= -180 && angle <= 180) {
    return angle;
  }

  return angle - 360 * std::round(angle / 360.0);
}
} // namespace Adhan

#endif /* MATHUTILS_HPP */