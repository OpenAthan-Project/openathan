#ifndef SHAFAQ_HPP
#define SHAFAQ_HPP

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Adhan {

/**
 * @brief Shafaq is the twilight in the sky. Different madhabs define the
 * appearance of twilight differently. These values are used by the
 * MoonsightingComittee method for the different ways to calculate Isha.
 */
enum class Shafaq : std::int8_t {
  /**
   * @brief General is a combination of Ahmer and Abyad.
   */
  General,

  /**
   * @brief Ahmer means the twilight is the red glow in the sky. Used by the
   * Shafi, Maliki, and Hanbali madhabs.
   */
  Ahmer,

  /**
   * @brief Abyad means the twilight is the white glow in the sky. Used by the
   * Hanafi madhab.
   */
  Abyad,
};

namespace ShafaqUtils {

constexpr std::string_view to_string(Shafaq sfq) {
  switch (sfq) {

  case Shafaq::General:
    return "General";
  case Shafaq::Ahmer:
    return "Ahmer";
  case Shafaq::Abyad:
    return "Abyad";
  }
  throw std::logic_error("Invalid shafaq");
}

constexpr Shafaq from_string(std::string_view s) {
  if (s == "General") {
    return Shafaq::General;
  }
  if (s == "Ahmer") {
    return Shafaq::Ahmer;
  }
  if (s == "Abyad") {
    return Shafaq::Abyad;
  }

  throw std::logic_error("Invalid shafaq");
}

} // namespace ShafaqUtils
} // namespace Adhan

#endif // SHAFAQ_HPP