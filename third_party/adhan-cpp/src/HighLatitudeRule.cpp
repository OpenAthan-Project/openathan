#include <adhan/HighLatitudeRule.hpp>

namespace Adhan {

HighLatitudeRule recommended(const Coordinates &coordinates) {
  if (coordinates.latitude > 48) {
    return HighLatitudeRule::SeventhOfTheNight;
  }
  return HighLatitudeRule::MiddleOfTheNight;
}
} // namespace Adhan
