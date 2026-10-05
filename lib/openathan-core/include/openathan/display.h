#pragma once
#include "scheduler.h"

namespace openathan {
struct SavedDisplay {
  uint32_t revision{};
  uint8_t brightness_percent{10};
  bool operator==(const SavedDisplay &) const = default;
};
using DisplayRecord = std::array<uint8_t, 16>;
DisplayRecord encode_display(const SavedDisplay &value);
bool decode_display(const DisplayRecord &record, SavedDisplay &value);
class DisplayStore {
 public:
  virtual ~DisplayStore() = default;
  virtual LoadResult load(SavedDisplay &value) = 0;
  virtual bool save(const SavedDisplay &value) = 0;
};
enum class DisplayResult { SAVED, UNCHANGED, INVALID, CONFLICT, STORAGE };
class DisplayPreferences {
 public:
  explicit DisplayPreferences(DisplayStore &store) : store_(store) {}
  void begin();
  DisplayResult update(uint8_t brightness_percent, uint32_t revision);
  const std::optional<SavedDisplay> &saved() const { return saved_; }
  bool writable() const { return writable_; }
  uint8_t brightness_percent() const { return saved_ ? saved_->brightness_percent : 10; }
 private:
  DisplayStore &store_;
  std::optional<SavedDisplay> saved_;
  bool writable_{};
};
class DisplayOutput {
 public:
  virtual ~DisplayOutput() = default;
  virtual bool apply(uint8_t brightness_percent) = 0;
};
}  // namespace openathan
