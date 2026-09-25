#pragma once
#include "playback.h"
#include "prayer_calculator.h"
#include <limits>
#include <string>
#include <vector>

namespace openathan {
enum class Prayer : uint8_t { FAJR, DHUHR, ASR, MAGHRIB, ISHA };
const char *prayer_name(Prayer prayer);
struct CivilDate {
  int year{};
  unsigned month{}, day{};
};
// Serial days since 1970-01-01; event identity never depends on adjusted time.
int32_t day_number(CivilDate date);
CivilDate civil_date(int32_t day);
bool valid_date(CivilDate date);
struct EventKey {
  int32_t day{};
  Prayer prayer{};
  bool operator==(const EventKey &) const = default;
};
struct Event {
  EventKey key;
  int64_t utc{};
  bool enabled{};
  // A coincident Isha/Fajr occurrence retains both durable identities.
  std::optional<EventKey> shared_with;
};
enum class ConflictResolution { SHARED_PLAYBACK, ISHA_SUPPRESSED };
struct ScheduleConflict {
  EventKey isha;
  int64_t isha_utc{};
  EventKey fajr;
  int64_t fajr_utc{};
  ConflictResolution resolution;
};
std::string describe_conflict(const ScheduleConflict &conflict);
struct Settings {
  double latitude{}, longitude{};
  Method method{Method::MUSLIM_WORLD_LEAGUE};
  bool hanafi{false};
  HighLatitudeRule high_latitude{HighLatitudeRule::AUTO};
  std::array<int, 6> offsets{};  // fajr, sunrise, dhuhr, asr, maghrib, isha
  std::array<bool, 5> enabled{true, true, true, true, true};
  bool operator==(const Settings &) const = default;
};
bool valid_settings(const Settings &settings);
struct ClockSample {
  bool valid{};
  int64_t utc{};
  uint64_t monotonic_ms{};
  CivilDate local_date;
  uint16_t subsecond_ms{};  // fractional part of UTC, sampled with monotonic_ms
};
class Clock {
 public:
  virtual ~Clock() = default;
  virtual ClockSample read() = 0;
};
class DayCalculator {
 public:
  virtual ~DayCalculator() = default;
  virtual bool calculate(const Settings &settings, CivilDate date, PrayerDay &out);
};
constexpr int32_t NEVER_CONSUMED = std::numeric_limits<int32_t>::min();
struct DurableState {
  std::array<int32_t, 5> consumed_through{NEVER_CONSUMED, NEVER_CONSUMED, NEVER_CONSUMED,
                                        NEVER_CONSUMED, NEVER_CONSUMED};
  std::optional<EventKey> skip;
  bool operator==(const DurableState &) const = default;
};
enum class LoadResult { EMPTY, LOADED, ERROR };
class StateStore {
 public:
  virtual ~StateStore() = default;
  virtual LoadResult load(DurableState &state) = 0;
  virtual bool save(const DurableState &state) = 0;  // must be durable before returning true
};
enum class Fault { NONE, INVALID_SETTINGS, INVALID_SCHEDULE, STORAGE, AUDIO_UNAVAILABLE, PLAYBACK_REJECTED };
const char *fault_name(Fault fault);
struct SchedulerStatus {
  bool clock_ready{};
  bool automatic_ready{};
  bool playing{};
  std::optional<Event> next;
  std::optional<EventKey> skip;
  Fault fault{Fault::NONE};
  std::vector<ScheduleConflict> conflicts;
};

class Scheduler {
 public:
  Scheduler(Clock &clock, DayCalculator &calculator, StateStore &store, Playback &playback)
      : clock_(clock), calculator_(calculator), store_(store), playback_(playback) {}
  bool begin(const Settings &settings);
  bool configure(const Settings &settings);
  bool validate_schedule(const Settings &settings, CivilDate date) const;
  void block_storage();
  void allow_playback(bool allowed) { playback_allowed_ = allowed; }
  void tick();
  bool skip_next();
  bool cancel_skip();
  void stop() { playback_.stop(); }
  SchedulerStatus status() const;
  const std::array<int32_t, 5> &consumed_through() const { return state_.consumed_through; }
 private:
  bool rebuild(CivilDate date);
  bool persist(const DurableState &state);
  void refresh_next(int64_t now);
  Clock &clock_;
  DayCalculator &calculator_;
  StateStore &store_;
  Playback &playback_;
  Settings settings_;
  DurableState state_;
  std::vector<Event> events_;
  std::vector<ScheduleConflict> conflicts_;
  std::optional<Event> next_;
  ClockSample previous_;
  int32_t built_day_{NEVER_CONSUMED};
  bool storage_ok_{false}, configured_{false}, armed_{false}, clock_ready_{false};
  bool playback_allowed_{true};
  Fault fault_{Fault::NONE};
};
}  // namespace openathan
