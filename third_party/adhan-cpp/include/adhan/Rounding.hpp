#ifndef ROUNDING_HPP
#define ROUNDING_HPP

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Adhan {

enum class Rounding : std::int8_t {
  Nearest,
  Up,
  None,
};

namespace RoundingUtils {

/**
 * @brief Given a Rounding (e.g.: Rounding::Nearest), returns a std::string_view
 *
 * @param r
 * @return constexpr std::string_view
 */
constexpr std::string_view to_string(Rounding r) {
  switch (r) {
  case Rounding::Nearest:
    return "Nearest";
  case Rounding::Up:
    return "Up";
  case Rounding::None:
    return "None";
  }
  throw std::logic_error("Invalid rounding");
}

/**
 * @brief Given a string (e.g.: "nearest"), returns a `Rounding`
 *
 * @param s
 * @return constexpr Rounding
 * @throws std::logic_error if the string names no rounding mode.
 */
constexpr Rounding from_string(std::string_view s) {
  if (s == "Nearest") {
    return Rounding::Nearest;
  }
  if (s == "Up") {
    return Rounding::Up;
  }
  if (s == "None") {
    return Rounding::None;
  }
  throw std::logic_error("Invalid rounding");
}

} // namespace RoundingUtils
} // namespace Adhan

#endif /* ROUNDING_HPP */