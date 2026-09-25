#ifndef DATEUTILS_HPP
#define DATEUTILS_HPP

#include "Rounding.hpp"

#include <chrono>
#include <optional>

namespace Adhan {

/**
 * @brief A point in time, in UTC, at millisecond resolution.
 *
 * Millisecond resolution is deliberate. A JavaScript Date holds
 * milliseconds, so the ported arithmetic truncates in the same place
 * the upstream library does.
 *
 * None of this needs a time zone database. Callers who want a local
 * wall clock reading do the conversion themselves, for example with
 * std::chrono::zoned_time.
 */
using Instant = std::chrono::sys_time<std::chrono::milliseconds>;

/**
 * @brief An Instant that may be absent.
 *
 * Upstream hands back an Invalid Date when a time cannot be worked out,
 * which mostly happens near the poles. std::nullopt says the same thing,
 * except the compiler makes you handle it.
 */
using OptInstant = std::optional<Instant>;

/**
 * @brief The current time, truncated to milliseconds.
 */
Instant now();

/**
 * @brief Shifts a calendar date by a whole number of days.
 *
 * Overflow rolls over, so day 32 of January lands on the 1st of February.
 * Negative counts move backwards.
 *
 * @param date Date to shift.
 * @param days Days to add, may be negative.
 * @return The shifted date.
 */
std::chrono::year_month_day
dateByAddingDays(const std::chrono::year_month_day &date, int days);

/**
 * @brief Shifts an instant by a number of minutes.
 *
 * @param date Instant to shift, may be absent.
 * @param minutes Minutes to add, may be fractional or negative.
 * @return The shifted instant, or nullopt if the input was absent or the
 *         result is out of range.
 */
OptInstant dateByAddingMinutes(const OptInstant &date, double minutes);

/**
 * @brief Shifts an instant by a number of seconds.
 *
 * The fractional part is truncated toward zero once it has been converted
 * to milliseconds, which is what the Date constructor does with a number.
 * A non-finite shift gives nullopt, the same way NaN gives an Invalid Date.
 *
 * @param date Instant to shift, may be absent.
 * @param seconds Seconds to add, may be fractional or negative.
 * @return The shifted instant, or nullopt if the input was absent or the
 *         result is out of range.
 */
OptInstant dateByAddingSeconds(const OptInstant &date, double seconds);

/**
 * @brief Drops the seconds field of an instant.
 *
 * @param date Instant to round, may be absent.
 * @param rounding Which way to go. Nearest rounds up from 30 seconds,
 *        Up always goes to the next minute, None leaves the value alone.
 * @return The rounded instant, or nullopt if the input was absent.
 */
OptInstant
roundedMinute(const OptInstant &date, Rounding rounding = Rounding::Nearest);

/**
 * @brief Whether a year has 366 days.
 */
bool isLeapYear(int year);

/**
 * @brief Position of a date within its year, where 1 January is day 1.
 */
int dayOfYear(const std::chrono::year_month_day &date);

/**
 * @brief Whether an instant was worked out successfully.
 */
bool isValidDate(const OptInstant &date);

/**
 * @brief Seconds elapsed from one instant to another.
 *
 * @param later The later instant.
 * @param earlier The earlier instant.
 * @return The gap in seconds, or NaN if either side is absent. NaN is what
 *         upstream ends up with when it subtracts an Invalid Date, and the
 *         callers here rely on that spreading through the arithmetic.
 */
double secondsBetween(const OptInstant &later, const OptInstant &earlier);

} // namespace Adhan

#endif // DATEUTILS_HPP
