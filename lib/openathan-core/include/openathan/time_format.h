#pragma once
#include "scheduler.h"
#include <string_view>

namespace openathan {
struct SavedTimeFormat {
  uint32_t revision{};
  uint8_t hours{24};
  bool operator==(const SavedTimeFormat &) const = default;
};
using TimeFormatRecord = std::array<uint8_t, 16>;
TimeFormatRecord encode_time_format(const SavedTimeFormat &value);
bool decode_time_format(const TimeFormatRecord &record, SavedTimeFormat &value);
class TimeFormatStore {
 public:
  virtual ~TimeFormatStore() = default;
  virtual LoadResult load(SavedTimeFormat &value) = 0;
  virtual bool save(const SavedTimeFormat &value) = 0;
};
enum class TimeFormatResult { SAVED, UNCHANGED, INVALID, CONFLICT, STORAGE };
class TimeFormatPreferences {
 public:
  explicit TimeFormatPreferences(TimeFormatStore &store) : store_(store) {}
  void begin();
  TimeFormatResult update(uint8_t hours, uint32_t revision);
  const std::optional<SavedTimeFormat> &saved() const { return saved_; }
  bool writable() const { return writable_; }
  uint8_t hours() const { return saved_ ? saved_->hours : 24; }
 private:
  TimeFormatStore &store_;
  std::optional<SavedTimeFormat> saved_;
  bool writable_{};
};
// Presentation only: input is already converted to the saved local timezone.
std::array<char, 9> format_clock(std::string_view hhmm, uint8_t hours);
}  // namespace openathan
