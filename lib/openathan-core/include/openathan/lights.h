#pragma once
#include "scheduler.h"

namespace openathan {
struct LightSettings {
  bool enabled{true};
  uint8_t brightness_percent{20};
  bool operator==(const LightSettings &) const = default;
};
struct SavedLights {
  uint32_t revision{};
  LightSettings value;
  bool operator==(const SavedLights &) const = default;
};
using LightRecord = std::array<uint8_t, 16>;
LightRecord encode_lights(const SavedLights &value);
bool decode_lights(const LightRecord &record, SavedLights &value);
class LightStore {
 public:
  virtual ~LightStore() = default;
  virtual LoadResult load(SavedLights &value) = 0;
  virtual bool save(const SavedLights &value) = 0;
};
enum class LightSaveResult { SAVED, UNCHANGED, INVALID, CONFLICT, STORAGE };
class LightPreferences {
 public:
  explicit LightPreferences(LightStore &store) : store_(store) {}
  void begin();
  LightSaveResult update(LightSettings value, uint32_t revision);
  const std::optional<SavedLights> &saved() const { return saved_; }
  bool writable() const { return writable_; }
 private:
  LightStore &store_;
  std::optional<SavedLights> saved_;
  bool writable_{};
};
struct LightFrame {
  uint8_t red{}, green{}, blue{}, brightness{};
  bool operator==(const LightFrame &) const = default;
};
// Optional capability: a failed or absent output never gates audio/scheduling.
class LightOutput {
 public:
  virtual ~LightOutput() = default;
  virtual bool apply(LightFrame frame) = 0;
};
enum class LightMode { OFF, GREEN, ORANGE, RED, PLAYING, WAITING, FAULT };
const char *light_mode_name(LightMode mode);
struct LightInputs {
  bool maintenance{}, fault{}, setup_complete{}, clock_ready{}, playing{};
  std::optional<int64_t> next_utc;
  int64_t utc{};
};
LightMode light_mode(const LightSettings &settings, const LightInputs &inputs);
LightMode prayer_proximity(int64_t remaining_seconds);
LightFrame light_frame(LightMode mode, uint8_t brightness, uint64_t monotonic_ms);
// Raw calculated prayer times, independent of audible eligibility and history.
class LightSchedule {
 public:
  bool rebuild(DayCalculator &calculator, const Settings &settings, CivilDate date);
  std::optional<int64_t> next(int64_t utc) const;
  // Read-only identity/eligibility view for displays; timestamp selection stays
  // identical to next(), including muted and suppressed calculated prayers.
  std::optional<Event> next_event(int64_t utc) const;
 private:
  std::array<std::optional<int64_t>, 15> times_{};
  std::optional<int64_t> previous_isha_, following_fajr_;
  int32_t first_day_{};
  std::array<bool, 5> enabled_{};
};
}  // namespace openathan
