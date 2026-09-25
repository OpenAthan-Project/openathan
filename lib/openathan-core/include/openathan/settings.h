#pragma once
#include "scheduler.h"
#include <functional>

namespace openathan {
enum class DstRuleType : uint8_t { NONE, MONTH_WEEK_DAY, JULIAN_NO_LEAP, DAY_OF_YEAR };
struct DstRule {
  int32_t time_seconds{};
  uint16_t day{};
  DstRuleType type{};
  uint8_t month{}, week{}, day_of_week{};
  bool operator==(const DstRule &) const = default;
};
struct Timezone {
  std::string name;
  // POSIX convention: positive offsets are west of UTC.
  int32_t standard_offset{}, daylight_offset{};
  DstRule start, end;
  bool operator==(const Timezone &) const = default;
};
struct DeviceSettings {
  Settings prayer;
  Timezone timezone;
  uint8_t volume{70};
  bool operator==(const DeviceSettings &) const = default;
};
struct SavedSettings {
  uint32_t revision{};
  DeviceSettings value;
  bool operator==(const SavedSettings &) const = default;
};
constexpr size_t TIMEZONE_NAME_SIZE = 96;
using SettingsRecord = std::array<uint8_t, 192>;
bool valid_timezone(const Timezone &timezone);
bool valid_device_settings(const DeviceSettings &settings);
SettingsRecord encode_settings(const SavedSettings &settings);
bool decode_settings(const SettingsRecord &record, SavedSettings &settings);
class SettingsStore {
 public:
  virtual ~SettingsStore() = default;
  virtual LoadResult load(SavedSettings &settings) = 0;
  virtual bool save(const SavedSettings &settings) = 0;
};
enum class SettingsResult { SAVED, UNCHANGED, CONFLICT, INVALID, INVALID_SCHEDULE, STORAGE };
const char *settings_result_name(SettingsResult result);
class SettingsService {
 public:
  explicit SettingsService(SettingsStore &store) : store_(store) {}
  bool begin(const DeviceSettings &defaults);
  SettingsResult update(const DeviceSettings &candidate, uint32_t expected_revision,
                        const std::function<bool(const DeviceSettings &)> &preflight);
  const std::optional<SavedSettings> &saved() const { return saved_; }
  bool healthy() const { return healthy_; }
 private:
  SettingsStore &store_;
  std::optional<SavedSettings> saved_;
  bool healthy_{false};
};
}  // namespace openathan
