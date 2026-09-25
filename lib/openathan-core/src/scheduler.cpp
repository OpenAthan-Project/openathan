#include "openathan/scheduler.h"
#include <algorithm>
#include <chrono>
#include <cmath>

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
  configured_ = false;
  bool valid = std::isfinite(settings.latitude) && std::isfinite(settings.longitude) &&
      std::abs(settings.latitude) <= 90 && std::abs(settings.longitude) <= 180 &&
      static_cast<unsigned>(settings.method) < 12 && static_cast<unsigned>(settings.high_latitude) < 3;
  for (int offset : settings.offsets) valid = valid && offset >= -120 && offset <= 120;
  if (!valid) {
    fault_ = storage_ok_ ? Fault::INVALID_SETTINGS : Fault::STORAGE;
    return false;
  }
  settings_ = settings;
  configured_ = true;
  fault_ = storage_ok_ ? Fault::NONE : Fault::STORAGE;
  return true;
}
bool Scheduler::rebuild(CivilDate date) {
  events_.clear();
  std::optional<int64_t> preceding;
  const int32_t today = day_number(date);
  // Check all available events, including disabled prayers and sunrise, for ordering.
  for (int32_t serial = today - 1; serial <= today + 1; ++serial) {
    PrayerDay times;
    if (!calculator_.calculate(settings_, civil_date(serial), times)) return false;
    unsigned prayer = 0;
    for (size_t i = 0; i < times.size(); ++i) {
      if (times[i]) {
        if (preceding && *times[i] <= *preceding) return false;
        preceding = times[i];
        if (i != 1)
          events_.push_back({{serial, static_cast<Prayer>(prayer)}, *times[i], settings_.enabled[prayer]});
      }
      if (i != 1) ++prayer;
    }
  }
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
    if (event.enabled && event.utc > now && event.key.day > state_.consumed_through[static_cast<unsigned>(event.key.prayer)]) {
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
    auto &watermark = candidate.consumed_through[static_cast<unsigned>(event.key.prayer)];
    if (event.utc > now.utc || event.key.day <= watermark) continue;
    const bool skipped = candidate.skip && *candidate.skip == event.key;
    if (continuous && event.utc > previous_.utc && (now.utc - event.utc) * 1000 + now.subsecond_ms <= 2000 &&
        event.enabled && !skipped)
      due = event;
    watermark = event.key.day;
  }
  if (candidate.skip) {
    const auto match = std::find_if(events_.begin(), events_.end(), [&](const Event &event) {
      return event.key == *candidate.skip && event.enabled && event.utc > now.utc &&
          event.key.day > candidate.consumed_through[static_cast<unsigned>(event.key.prayer)];
    });
    // A backward clock jump must not discard a future skip outside this 3-day window.
    if (match == events_.end() && candidate.skip->day <= today + 1) candidate.skip.reset();
  }
  if (!persist(candidate)) return;
  previous_ = now;
  armed_ = true;
  refresh_next(now.utc);
  if (!playback_.ready()) {
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
  return {clock_ready_, armed_ && configured_ && storage_ok_ && playback_.ready(),
          playback_.playing(), next_, state_.skip, fault_};
}
}  // namespace openathan
