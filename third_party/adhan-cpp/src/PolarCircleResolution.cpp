#include <adhan/DateUtils.hpp>
#include <adhan/PolarCircleResolution.hpp>

#include <cmath>

namespace Adhan {

namespace {

/* Degrees to add/remove at each resolution step */
const double LATITUDE_VARIATION_STEP = 0.5;

/* Based on https://en.wikipedia.org/wiki/Midnight_sun */
const double UNSAFE_LATITUDE = 65;

bool isValidSolarTime(const SolarTime &solarTime) {
  return !std::isnan(solarTime.sunrise) && !std::isnan(solarTime.sunset);
}

/**
 * Same as Math.sign in JS. Returns -1, 0 or 1, unlike std::copysign,
 * which calls 0 positive.
 */
double jsSign(double x) {
  if (x > 0) {
    return 1;
  }
  if (x < 0) {
    return -1;
  }
  return 0;
}

std::optional<PolarCircleResolver> aqrabYaumResolver(
    const Coordinates &coordinates, const std::chrono::year_month_day &date,
    int daysAdded = 1,
    int direction = 1) {
  if (daysAdded > static_cast<int>(std::ceil(365 / 2.0))) {
    return std::nullopt;
  }

  const auto testDate = dateByAddingDays(date, direction * daysAdded);
  const auto tomorrow = dateByAddingDays(testDate, 1);
  SolarTime solarTime(testDate, coordinates);
  SolarTime tomorrowSolarTime(tomorrow, coordinates);

  if (!isValidSolarTime(solarTime) || !isValidSolarTime(tomorrowSolarTime)) {
    return aqrabYaumResolver(
        coordinates, date, daysAdded + (direction > 0 ? 0 : 1), -direction);
  }

  return PolarCircleResolver{
      .date = date,
      .tomorrow = tomorrow,
      .coordinates = coordinates,
      .solarTime = solarTime,
      .tomorrowSolarTime = tomorrowSolarTime,
  };
}

std::optional<PolarCircleResolver> aqrabBaladResolver(
    const Coordinates &coordinates, const std::chrono::year_month_day &date,
    double latitude) {
  const Coordinates adjusted(latitude, coordinates.longitude);
  SolarTime solarTime(date, adjusted);
  const auto tomorrow = dateByAddingDays(date, 1);
  SolarTime tomorrowSolarTime(tomorrow, adjusted);

  if (!isValidSolarTime(solarTime) || !isValidSolarTime(tomorrowSolarTime)) {
    if (std::abs(latitude) >= UNSAFE_LATITUDE) {
      return aqrabBaladResolver(
          coordinates, date,
          latitude - jsSign(latitude) * LATITUDE_VARIATION_STEP);
    }
    return std::nullopt;
  }

  return PolarCircleResolver{
      .date = date,
      .tomorrow = tomorrow,
      .coordinates = Coordinates(latitude, coordinates.longitude),
      .solarTime = solarTime,
      .tomorrowSolarTime = tomorrowSolarTime,
  };
}

} // namespace

PolarCircleResolver polarCircleResolvedValues(
    PolarCircleResolution resolver, const std::chrono::year_month_day &date,
    const Coordinates &coordinates) {

#ifdef ADHAN_TESTING
  ++polarCircleResolvedValuesCallCount;
#endif

  auto makeDefault = [&]() {
    const auto tomorrow = dateByAddingDays(date, 1);
    return PolarCircleResolver{
        .date = date,
        .tomorrow = tomorrow,
        .coordinates = coordinates,
        .solarTime = SolarTime(date, coordinates),
        .tomorrowSolarTime = SolarTime(tomorrow, coordinates),
    };
  };

  switch (resolver) {
  case PolarCircleResolution::AqrabYaum: {
    auto result = aqrabYaumResolver(coordinates, date);
    return result ? *result : makeDefault();
  }
  case PolarCircleResolution::AqrabBalad: {
    const double latitude = coordinates.latitude;
    auto result = aqrabBaladResolver(
        coordinates, date,
        latitude - jsSign(latitude) * LATITUDE_VARIATION_STEP);
    return result ? *result : makeDefault();
  }
  default: {
    return makeDefault();
  }
  }
}
} // namespace Adhan