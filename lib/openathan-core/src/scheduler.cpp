#include "openathan/scheduler.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace openathan {
using namespace std::chrono;
bool valid_date(CivilDate d) {
  return d.year >= 1900 && d.year <= 2100 && d.month >= 1 && d.month <= 12 && d.day >= 1 && d.day <= 31 &&
         year_month_day{year(d.year), month(d.month), day(d.day)}.ok();
}
int32_t day_number(CivilDate d) {
  return static_cast<int32_t>(sys_days{year(d.year)/month(d.month)/day(d.day)}.time_since_epoch().count());
}
CivilDate civil_date(int32_t serial) {
  const year_month_day d{sys_days{days{serial}}};
  return {int(d.year()), unsigned(d.month()), unsigned(d.day())};
}
const char *prayer_name(Prayer prayer) {
  constexpr const char *names[] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
  const auto index = static_cast<unsigned>(prayer);
  return index < 5 ? names[index] : "Unknown";
}
const char *fault_name(Fault fault) {
  switch (fault) {
    case Fault::NONE: return "none";
    case Fault::INVALID_SETTINGS: return "invalid settings";
    case Fault::INVALID_SCHEDULE: return "invalid prayer schedule";
    case Fault::STORAGE: return "durable state unavailable";
    case Fault::AUDIO_UNAVAILABLE: return "audio unavailable";
    case Fault::PLAYBACK_REJECTED: return "playback request rejected";
  }
  return "unknown";
}
std::string describe_conflict(const ScheduleConflict &conflict) {
  const auto isha = civil_date(conflict.isha.day), fajr = civil_date(conflict.fajr.day);
  char text[192];
  std::snprintf(text, sizeof(text), "Isha %04d-%02u-%02u UTC=%lld; Fajr %04d-%02u-%02u UTC=%lld: %s",
      isha.year, isha.month, isha.day, static_cast<long long>(conflict.isha_utc),
      fajr.year, fajr.month, fajr.day, static_cast<long long>(conflict.fajr_utc),
      conflict.resolution == ConflictResolution::SHARED_PLAYBACK ? "shared playback" : "Isha suppressed");
  return text;
}
static bool matches(const Event &event, EventKey key) {
  return event.key == key || (event.shared_with && *event.shared_with == key);
}
static bool consumed(const Event &event, const DurableState &state) {
  const auto handled = [&](EventKey key) {
    return key.day <= state.consumed_through[static_cast<unsigned>(key.prayer)];
  };
  // A settings change cannot replay an already handled member as a shared event.
  return handled(event.key) || (event.shared_with && handled(*event.shared_with));
}
bool DayCalculator::calculate(const Settings &s, CivilDate d, PrayerDay &out) {
  return calculate_prayers({s.latitude, s.longitude, d.year, d.month, d.day, s.method,
                            s.hanafi, s.high_latitude, s.offsets}, out);
}
bool Scheduler::begin(const Settings &settings) {
  state_ = {};
  storage_ok_ = store_.load(state_) != LoadResult::ERROR;
  if (!storage_ok_) fault_ = Fault::STORAGE;
  return configure(settings) && storage_ok_;
}
bool Scheduler::configure(const Settings &settings) {
  armed_ = false;
  built_day_ = NEVER_CONSUMED;
  next_.reset();
  events_.clear();
  conflicts_.clear();
  configured_ = false;
  if (!valid_settings(settings)) {
    fault_ = storage_ok_ ? Fault::INVALID_SETTINGS : Fault::STORAGE;
    return false;
  }
  settings_ = settings;
  configured_ = true;
  fault_ = storage_ok_ ? Fault::NONE : Fault::STORAGE;
  return true;
}
bool valid_settings(const Settings &settings) {
  bool valid = std::isfinite(settings.latitude) && std::isfinite(settings.longitude) &&
      std::abs(settings.latitude) <= 90 && std::abs(settings.longitude) <= 180 &&
      static_cast<unsigned>(settings.method) < 12 &&
      static_cast<unsigned>(settings.high_latitude) <= static_cast<unsigned>(HighLatitudeRule::AUTO);
  for (int offset : settings.offsets) valid = valid && offset >= -120 && offset <= 120;
  return valid;
}
bool Scheduler::validate_schedule(const Settings &settings, CivilDate date) const {
  // A scratch timetable never loads/saves consumption or calls playback.
  Scheduler preview(clock_, calculator_, store_, playback_);
  return valid_date(date) && preview.configure(settings) && preview.rebuild(date);
}
void Scheduler::block_storage() {
  storage_ok_ = false;
  armed_ = false;
  next_.reset();
  fault_ = Fault::STORAGE;
}
bool Scheduler::rebuild(CivilDate date) {
  // Clearing the timetable invalidates its cached date, including on failure.
  built_day_ = NEVER_CONSUMED;
  events_.clear();
  conflicts_.clear();
  const int32_t today = day_number(date);
  // Guard days classify both edges of the three-day playback window. They do
  // not add independent playback events, and need not extend the supported dates.
  std::array<PrayerDay, 5> days{};
  std::optional<int64_t> preceding;
  size_t preceding_index = 0;
  size_t preceding_day = 0;
  for (size_t d = 0; d < days.size(); ++d) {
    const auto day = civil_date(today - 2 + static_cast<int32_t>(d));
    if (!valid_date(day) && (d == 0 || d == 4)) continue;
    if (!calculator_.calculate(settings_, day, days[d])) return false;
    bool first = true;
    for (size_t i = 0; i < days[d].size(); ++i) {
      if (!days[d][i]) continue;
      // Only an adjacent-day Isha/Fajr boundary may overlap. Disabled events
      // and sunrise still participate in all other ordering validation.
      if (preceding && *days[d][i] <= *preceding) {
        if (!(first && preceding_day + 1 == d && preceding_index == 5 && i == 0)) return false;
        // The exception covers this pair only, not a night that also crosses
        // another prayer or sunrise on either side of the boundary.
        for (size_t j = 0; j < 5; ++j)
          if (days[d - 1][j] && *days[d - 1][j] >= *days[d][0]) return false;
        for (size_t j = 1; j < days[d].size(); ++j)
          if (days[d][j] && *days[d][j] <= *preceding) return false;
      }
      preceding = days[d][i];
      preceding_index = i;
      preceding_day = d;
      first = false;
    }
  }
  std::vector<Event> events;
  std::vector<ScheduleConflict> conflicts;
  for (size_t d = 0; d < days.size(); ++d) {
    const int32_t serial = today - 2 + static_cast<int32_t>(d);
    const auto &times = days[d];
    if (d + 1 < days.size() && times[5] && days[d + 1][0] && *times[5] >= *days[d + 1][0]) {
      conflicts.push_back({{serial, Prayer::ISHA}, *times[5], {serial + 1, Prayer::FAJR}, *days[d + 1][0],
          *times[5] == *days[d + 1][0] ? ConflictResolution::SHARED_PLAYBACK : ConflictResolution::ISHA_SUPPRESSED});
    }
    for (size_t i = 0; i < times.size(); ++i) {
      if (!times[i] || i == 1) continue;
      const size_t prayer = i == 0 ? 0 : i - 1;
      Event event{{serial, static_cast<Prayer>(prayer)}, *times[i], settings_.enabled[prayer], {}};
      if (i == 0 && d > 0 && days[d - 1][5] == times[0]) {
        event.shared_with = EventKey{serial - 1, Prayer::ISHA};
        if (!settings_.enabled[0] && settings_.enabled[4]) std::swap(event.key, *event.shared_with);
        event.enabled = settings_.enabled[0] || settings_.enabled[4];
      }
      if (i == 5 && d + 1 < days.size() && days[d + 1][0]) {
        if (times[5] == days[d + 1][0]) continue; // represented by the shared Fajr occurrence
        if (*times[5] > *days[d + 1][0]) event.enabled = false; // consume, but never announce late Isha
      }
      const auto in_window = [&](EventKey key) { return key.day >= today - 1 && key.day <= today + 1; };
      if (in_window(event.key) || (event.shared_with && in_window(*event.shared_with))) events.push_back(event);
    }
  }
  std::sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.utc < b.utc; });
  events_ = std::move(events);
  conflicts_ = std::move(conflicts);
  built_day_ = today;
  return true;
}
bool Scheduler::persist(const DurableState &candidate) {
  if (candidate == state_) return true;
  if (!store_.save(candidate)) {
    storage_ok_ = false;
    armed_ = false;
    next_.reset();
    fault_ = Fault::STORAGE;
    return false;
  }
  state_ = candidate;
  return true;
}
void Scheduler::refresh_next(int64_t now) {
  next_.reset();
  for (const auto &event : events_)
    if (event.enabled && event.utc > now && !consumed(event, state_)) {
      next_ = event;
      return;
    }
}
void Scheduler::tick() {
  const ClockSample now = clock_.read();
  clock_ready_ = now.valid && valid_date(now.local_date) && now.subsecond_ms < 1000;
  if (!clock_ready_ || !configured_ || !storage_ok_) {
    armed_ = false;
    next_.reset();
    return;
  }
  const auto utc_delta = now.utc - previous_.utc;
  const bool mono_backwards = now.monotonic_ms < previous_.monotonic_ms;
  const uint64_t elapsed = mono_backwards ? 0 : now.monotonic_ms - previous_.monotonic_ms;
  const int64_t wall_elapsed = utc_delta * 1000 + now.subsecond_ms - previous_.subsecond_ms;
  // Millisecond samples distinguish a small time step from normal polling delay.
  // Allow 250ms sampling/slew tolerance; larger corrections rearm without catch-up.
  const bool continuous = armed_ && !mono_backwards && elapsed <= 3000 && utc_delta >= 0 &&
      utc_delta <= 4 && wall_elapsed >= 0 && std::abs(wall_elapsed - static_cast<int64_t>(elapsed)) <= 250;
  const int32_t today = day_number(now.local_date);
  if (built_day_ != today) {
    if (!rebuild(now.local_date)) {
      events_.clear();
      next_.reset();
      armed_ = false;
      fault_ = Fault::INVALID_SCHEDULE;
      return;
    }
    fault_ = Fault::NONE;
  }
  DurableState candidate = state_;
  for (auto &watermark : candidate.consumed_through)
    watermark = std::max(watermark, today - 2);
  std::optional<Event> due;
  for (const auto &event : events_) {
    if (event.utc > now.utc) continue;
    const bool skipped = candidate.skip && matches(event, *candidate.skip);
    if (!consumed(event, candidate) && continuous && event.utc > previous_.utc &&
        (now.utc - event.utc) * 1000 + now.subsecond_ms <= 2000 &&
        event.enabled && !skipped)
      due = event;
    const auto consume = [&](EventKey key) {
      auto &watermark = candidate.consumed_through[static_cast<unsigned>(key.prayer)];
      watermark = std::max(watermark, key.day);
    };
    consume(event.key);
    if (event.shared_with) consume(*event.shared_with);
  }
  if (candidate.skip) {
    const auto match = std::find_if(events_.begin(), events_.end(), [&](const Event &event) {
      return matches(event, *candidate.skip) && event.enabled && event.utc > now.utc && !consumed(event, candidate);
    });
    // A backward clock jump must not discard a future skip outside this 3-day window.
    if (match == events_.end() && candidate.skip->day <= today + 1) candidate.skip.reset();
  }
  if (!persist(candidate)) return;
  previous_ = now;
  armed_ = true;
  refresh_next(now.utc);
  if (!playback_allowed_ || !playback_.ready()) {
    fault_ = Fault::AUDIO_UNAVAILABLE;
    return;  // due event remains consumed; never retry when audio recovers
  }
  if (fault_ == Fault::AUDIO_UNAVAILABLE) fault_ = Fault::NONE;
  if (due) fault_ = playback_.start(due->key.prayer == Prayer::FAJR ? Track::FAJR : Track::NORMAL)
                        ? Fault::NONE : Fault::PLAYBACK_REJECTED;
}
bool Scheduler::skip_next() {
  tick();
  if (!storage_ok_ || !armed_ || !next_) return false;
  DurableState candidate = state_;
  // Repeated presses do not move an already selected skip onto another prayer.
  if (candidate.skip) return true;
  candidate.skip = next_->key;
  return persist(candidate);
}
bool Scheduler::cancel_skip() {
  if (!storage_ok_) return false;
  DurableState candidate = state_;
  candidate.skip.reset();
  return persist(candidate);
}
SchedulerStatus Scheduler::status() const {
  return {clock_ready_, armed_ && configured_ && storage_ok_ && playback_allowed_ && playback_.ready(),
          playback_.playing(), next_, state_.skip, fault_, conflicts_};
}
}  // namespace openathan
