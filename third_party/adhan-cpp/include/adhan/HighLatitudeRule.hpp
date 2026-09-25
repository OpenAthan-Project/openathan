#ifndef HIGHLATITUDERULE_HPP
#define HIGHLATITUDERULE_HPP

#include "Coordinates.hpp"
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Adhan {

enum class HighLatitudeRule : std::int8_t {
  MiddleOfTheNight,
  SeventhOfTheNight,
  TwilightAngle,
};

namespace HighLatitudeRuleUtils {
/**
 * @brief Given a HighLatitudeRule (e.g.: HighLatitudeRule::MiddleOfTheNight),
 * returns a std::string_view
 *
 * @param r
 * @return constexpr std::string_view
 */
constexpr std::string_view to_string(HighLatitudeRule h) {
  switch (h) {

  case HighLatitudeRule::MiddleOfTheNight:
    return "MiddleOfTheNight";
  case HighLatitudeRule::SeventhOfTheNight:
    return "SeventhOfTheNight";
  case HighLatitudeRule::TwilightAngle:
    return "TwilightAngle";
  }

  throw std::logic_error("Invalid high latitude rule");
}

/**
 * @brief Given a string (e.g.: "MiddleOfTheNight"), returns a
 * `HighLatitudeRule`
 *
 * @param s
 * @return constexpr HighLatitudeRule
 */
constexpr HighLatitudeRule from_string(std::string_view s) {
  if (s == "MiddleOfTheNight") {
    return HighLatitudeRule::MiddleOfTheNight;
  }
  if (s == "SeventhOfTheNight") {
    return HighLatitudeRule::SeventhOfTheNight;
  }
  if (s == "TwilightAngle") {
    return HighLatitudeRule::TwilightAngle;
  }

  throw std::logic_error("Invalid high latitude rule");
}
} // namespace HighLatitudeRuleUtils

HighLatitudeRule recommended(const Coordinates &coordinates);
} // namespace Adhan

#endif /* HIGHLATITUDERULE_HPP */