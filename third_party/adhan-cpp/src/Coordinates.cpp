#include <adhan/Coordinates.hpp>
#include <stdexcept>

namespace Adhan {

Coordinates::Coordinates(double latitude, double longitude) {
  if (latitude < -90.0 || latitude > 90.0) {
    throw std::invalid_argument("Coordinates: latitude must be in [-90, 90]");
  }
  if (longitude < -180.0 || longitude > 180.0) {
    throw std::invalid_argument(
        "Coordinates: longitude must be in [-180, 180]");
  }
  this->latitude = latitude;
  this->longitude = longitude;
};
} // namespace Adhan