#include "openathan/prayer_calculator.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void check(bool condition) { if (!condition) std::abort(); }
int main() {
  using namespace openathan;
  using namespace std::chrono;
  PrayerRequest r{35.775, -78.6336, 2015, 7, 12, Method::NORTH_AMERICA, true};
  PrayerDay out;
  check(calculate_prayers(r, out));
  // Published upstream v1.0.2 North America/Hanafi reference, UTC minutes.
  const int minutes[] = {8*60+42, 10*60+8, 17*60+21, 22*60+22, 24*60+32, 25*60+57};
  const auto midnight = duration_cast<seconds>(sys_days{2015y/July/12d}.time_since_epoch()).count();
  for (size_t i = 0; i < out.size(); ++i) check(out[i] && *out[i] == midnight + minutes[i]*60);
  auto standard = r;
  standard.hanafi = false;
  PrayerDay standard_day;
  check(calculate_prayers(standard, standard_day) && standard_day[3] < out[3]);
  for (unsigned method = 0; method < 12; ++method) {
    r.method = static_cast<Method>(method);
    check(calculate_prayers(r, out));
    for (const auto &event : out) check(event.has_value());
  }
  r.latitude = std::numeric_limits<double>::quiet_NaN();
  check(!calculate_prayers(r, out));
  for (const auto &event : out) check(!event);
  r.latitude = 91;
  check(!calculate_prayers(r, out));
  r.latitude = 0; r.longitude = std::numeric_limits<double>::infinity();
  check(!calculate_prayers(r, out));
  r.longitude = 0; r.month = 2; r.day = 30;
  check(!calculate_prayers(r, out));
  r.day = 20; r.method = static_cast<Method>(100);
  check(!calculate_prayers(r, out));
  r = {89, 0, 2026, 6, 21, Method::MUSLIM_WORLD_LEAGUE, false};
  check(calculate_prayers(r, out));
  check(!out[1] && !out[4]);  // no invented sunrise/sunset during polar day
  r = {35.775, -78.6336, 2015, 7, 12, Method::NORTH_AMERICA, true};
  PrayerDay original;
  check(calculate_prayers(r, original));
  for (unsigned rule=0; rule<3; ++rule) {
    r.high_latitude=static_cast<HighLatitudeRule>(rule);
    check(calculate_prayers(r,out));
  }
  auto high = PrayerRequest{51.5074, -0.1278, 2026, 6, 21, Method::MUSLIM_WORLD_LEAGUE};
  // Delegate the threshold and hemisphere behavior exactly to upstream:
  // greater than 48 north uses seventh; all other latitudes use middle.
  for (double latitude : {-60.0, -48.1, 0.0, 48.0, 48.0001, 60.0}) {
    high.latitude = latitude;
    high.high_latitude = HighLatitudeRule::AUTO;
    PrayerDay automatic, explicit_day;
    check(calculate_prayers(high, automatic));
    high.high_latitude = latitude > 48 ? HighLatitudeRule::SEVENTH_OF_NIGHT : HighLatitudeRule::MIDDLE_OF_NIGHT;
    check(calculate_prayers(high, explicit_day) && automatic == explicit_day);
  }
  high.latitude = 51.5074;
  std::array<PrayerDay, 3> rules;
  for (unsigned rule=0; rule<3; ++rule) {
    high.high_latitude = static_cast<HighLatitudeRule>(rule);
    check(calculate_prayers(high, rules[rule]));
  }
  check(rules[0][0] != rules[1][0] && rules[1][0] != rules[2][0] && rules[0][0] != rules[2][0]);
  high.high_latitude = static_cast<HighLatitudeRule>(4);
  check(!calculate_prayers(high, out));
  r.high_latitude=HighLatitudeRule::MIDDLE_OF_NIGHT;
  r.offsets[0]=10;
  check(calculate_prayers(r,out) && out[0] == *original[0]+600);
  r.offsets[0]=121;
  check(!calculate_prayers(r,out));
  r.offsets[0]=120; // pushes Fajr after sunrise in this fixture
  check(!calculate_prayers(r,out));
  for (const auto &event : out) check(!event);
  std::puts("Prayer reference, presets, input rejection and polar cases passed");
}
