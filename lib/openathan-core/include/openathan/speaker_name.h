#pragma once
#include "scheduler.h"
#include <string_view>

namespace openathan {
// No network identity or scheduler state belongs to this preference.
bool normalize_speaker_name(std::string_view input, std::string &name);
struct SavedSpeakerName {
  uint32_t revision{};
  std::string name;
  bool operator==(const SavedSpeakerName &) const = default;
};
using SpeakerNameRecord = std::array<uint8_t, 144>;
SpeakerNameRecord encode_speaker_name(const SavedSpeakerName &value);
bool decode_speaker_name(const SpeakerNameRecord &record, SavedSpeakerName &value);
class SpeakerNameStore {
 public:
  virtual ~SpeakerNameStore() = default;
  virtual LoadResult load(SavedSpeakerName &value) = 0;
  virtual bool save(const SavedSpeakerName &value) = 0;
};
enum class SpeakerNameResult { SAVED, UNCHANGED, INVALID, CONFLICT, STORAGE };
class SpeakerNamePreferences {
 public:
  explicit SpeakerNamePreferences(SpeakerNameStore &store) : store_(store) {}
  void begin();
  SpeakerNameResult update(std::string_view name, uint32_t revision);
  const std::optional<SavedSpeakerName> &saved() const { return saved_; }
  bool writable() const { return writable_; }
 private:
  SpeakerNameStore &store_;
  std::optional<SavedSpeakerName> saved_;
  bool writable_{};
};
}  // namespace openathan
