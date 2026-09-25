#ifndef MADHAB_HPP
#define MADHAB_HPP

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Adhan {

enum class Madhab : std::int8_t {
  Shafi,
  Hanafi,
};

namespace MadhabUtils {
constexpr std::string_view to_string(Madhab m) {
  switch (m) {
  case Madhab::Shafi:
    return "Shafi";
  case Madhab::Hanafi:
    return "Hanafi";
  }
  throw std::logic_error("Invalid madhab");
}

constexpr Madhab from_string(std::string_view s) {

  if (s == "Shafi") {
    return Madhab::Shafi;
  }
  if (s == "Hanafi") {
    return Madhab::Hanafi;
  }

  throw std::logic_error("Invalid madhab");
}

} // namespace MadhabUtils

constexpr int shadow_length(Madhab madhab) {
  switch (madhab) {
  case Madhab::Shafi:
    return 1;
  case Madhab::Hanafi:
    return 2;
  }
  throw std::logic_error("Invalid Madhab");
}

constexpr int shadow_length(std::string_view s) {
  return shadow_length(MadhabUtils::from_string(s));
}
} // namespace Adhan

#endif /* MADHAB_HPP */