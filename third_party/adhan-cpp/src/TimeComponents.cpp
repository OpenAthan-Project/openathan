#include <adhan/TimeComponents.hpp>

#include <chrono>
#include <cmath>

namespace Adhan {

/**
 * Note:
 * If validation rules increase in number, the `valid_` initialization
 * should be put into the constructor body
 */

TimeComponents::TimeComponents(double num) : valid_(std::isfinite(num)) {

  if (!valid_) {
    hours = 0;
    minutes = 0;
    seconds = 0;
    return;
  }

  hours = static_cast<int>(std::floor(num));
  minutes = static_cast<int>(std::floor((num - hours) * 60));
  seconds =
      static_cast<int>(std::floor((num - (hours + minutes / 60.0)) * 60 * 60));
}

OptInstant TimeComponents::utcDate(const std::chrono::year_month_day &date) const {
  if (!valid_) {
    return std::nullopt;
  }

  /**
   * Adding the components as durations, rather than stuffing them into a
   * calendar type, is what lets an out of range hour count roll into the
   * next or previous day on its own.
   */
  return Instant{
      std::chrono::sys_days{date} + std::chrono::hours{hours} +
      std::chrono::minutes{minutes} + std::chrono::seconds{seconds}};
}
} // namespace Adhan
