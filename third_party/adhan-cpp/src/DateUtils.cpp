#include <adhan/DateUtils.hpp>

#include <array>
#include <cmath>
#include <limits>

namespace Adhan {

namespace {

/**
 * Largest epoch value, in milliseconds either side of 1970, that a
 * JavaScript Date will accept. Anything past this is an Invalid Date
 * upstream, so it is nullopt here. The limit also keeps the value inside
 * the range a double represents exactly, which is why it was picked.
 */
constexpr double MAX_TIME_VALUE = 8.64e15;

} // namespace

Instant now() {
  return std::chrono::floor<std::chrono::milliseconds>(
      std::chrono::system_clock::now());
}

std::chrono::year_month_day
dateByAddingDays(const std::chrono::year_month_day &date, int days) {
  /**
   * Going through sys_days does the rollover for us, so there is no
   * carry logic to write by hand.
   */
  return std::chrono::year_month_day{
      std::chrono::sys_days{date} + std::chrono::days{days}};
}

OptInstant dateByAddingMinutes(const OptInstant &date, double minutes) {
  return dateByAddingSeconds(date, minutes * 60);
}

OptInstant dateByAddingSeconds(const OptInstant &date, double seconds) {
  if (!date) {
    return std::nullopt;
  }

  const double shift = seconds * 1000.0;
  const double total =
      static_cast<double>(date->time_since_epoch().count()) + shift;

  /**
   * Casting a NaN or an out of range double to an integer is undefined
   * behaviour in C++, so both cases are caught before the cast. JS just
   * hands back an Invalid Date instead.
   */
  if (!std::isfinite(total) || std::abs(total) > MAX_TIME_VALUE) {
    return std::nullopt;
  }

  return Instant{
      std::chrono::milliseconds{static_cast<long long>(total)}};
}

OptInstant roundedMinute(const OptInstant &date, Rounding rounding) {
  if (!date) {
    return std::nullopt;
  }

  const auto dayStart = std::chrono::floor<std::chrono::days>(*date);
  const auto secondOfDay =
      std::chrono::floor<std::chrono::seconds>(*date - dayStart);
  const int seconds =
      static_cast<int>((secondOfDay % std::chrono::minutes{1}).count());

  int offset = (seconds >= 30) ? (60 - seconds) : (-seconds);
  if (rounding == Rounding::Up) {
    offset = 60 - seconds;
  } else if (rounding == Rounding::None) {
    offset = 0;
  }

  return dateByAddingSeconds(date, offset);
}

bool isLeapYear(int year) {
  if (year % 4 != 0) {
    return false;
  }
  if (year % 100 == 0 && year % 400 != 0) {
    return false;
  }
  return true;
}

int dayOfYear(const std::chrono::year_month_day &date) {
  const int year = static_cast<int>(date.year());
  const int feb = isLeapYear(year) ? 29 : 28;
  const std::array<int, 12> months = {31, feb, 31, 30, 31, 30,
                                      31, 31,  30, 31, 30, 31};

  const auto monthIndex = static_cast<unsigned>(date.month()) - 1;

  int result = 0;
  for (unsigned i = 0; i < monthIndex; i++) {
    result += months.at(i);
  }
  result += static_cast<int>(static_cast<unsigned>(date.day()));

  return result;
}

bool isValidDate(const OptInstant &date) {
  return date.has_value();
}

double secondsBetween(const OptInstant &later, const OptInstant &earlier) {
  if (!later || !earlier) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return (*later - *earlier) / std::chrono::duration<double>{1};
}

} // namespace Adhan
