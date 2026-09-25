#ifndef TIMECOMPONENTS_HPP
#define TIMECOMPONENTS_HPP

#include "DateUtils.hpp"

#include <chrono>

namespace Adhan {

/**
 * @brief Splits a fractional hour count into hours, minutes and seconds.
 *
 * The solar routines hand back times as a plain number of hours, which can
 * sit outside the 0 to 24 range or be negative. Those cases are fine, they
 * roll into the neighbouring day once the components are added to a date.
 */
class TimeComponents {
public:
  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
  int hours;
  int minutes;
  int seconds;
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

  /**
   * @param num Time of day as a fractional number of hours. A non-finite
   *        value marks the object invalid rather than throwing.
   */
  explicit TimeComponents(double num);

  /**
   * @brief Places these components on a calendar date, read as UTC.
   *
   * @param date The day to attach the components to.
   * @return The resulting instant, or nullopt if the components are invalid.
   */
  OptInstant utcDate(const std::chrono::year_month_day &date) const;

  /**
   * @brief Whether the source value was finite.
   */
  bool isValid() const {
    return valid_;
  }

private:
  bool valid_;
};
} // namespace Adhan

#endif // TIMECOMPONENTS_HPP
