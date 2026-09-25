#ifndef POLARCIRCLERESOLUTION_HPP
#define POLARCIRCLERESOLUTION_HPP

#include "Coordinates.hpp"
#include "SolarTime.hpp"

#include <chrono>
#include <cstdint>

namespace Adhan {

#ifdef ADHAN_TESTING

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
/**
 * Test-only call counter, standing in for vi.spyOn's call-tracking in the
 * TS test suite. Excluded entirely in release builds
 */
inline int polarCircleResolvedValuesCallCount = 0;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#endif

enum class PolarCircleResolution : std::int8_t {
  AqrabBalad,
  AqrabYaum,
  Unresolved,
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct PolarCircleResolver {
  std::chrono::year_month_day date;
  std::chrono::year_month_day tomorrow;
  Coordinates coordinates;
  SolarTime solarTime;
  SolarTime tomorrowSolarTime;
};

/**
 * @brief Finds a nearby day or latitude where sunrise and sunset both exist.
 *
 * @param resolver Which strategy to use. Unresolved returns the original
 *        day untouched.
 * @param date The day being calculated.
 * @param coordinates Observer position.
 * @return Solar figures for the substitute day or place. Falls back to the
 *         original values when no substitute is found.
 */
PolarCircleResolver polarCircleResolvedValues(
    PolarCircleResolution resolver, const std::chrono::year_month_day &date,
    const Coordinates &coordinates);

} // namespace Adhan

#endif // POLARCIRCLERESOLUTION_HPP