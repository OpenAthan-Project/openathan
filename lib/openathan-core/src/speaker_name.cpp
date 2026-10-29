#include "openathan/speaker_name.h"
#include <algorithm>
#include <limits>

namespace openathan {
namespace {
bool space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13); }
void put(SpeakerNameRecord &record, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) record[at + i] = value >> (8 * i);
}
uint32_t get(const SpeakerNameRecord &record, size_t at) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(record[at + i]) << (8 * i);
  return value;
}
uint32_t checksum(const SpeakerNameRecord &record) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < record.size() - 4; ++i) {
    value ^= record[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
}
bool normalize_speaker_name(std::string_view input, std::string &name) {
  while (!input.empty() && space(input.front())) input.remove_prefix(1);
  while (!input.empty() && space(input.back())) input.remove_suffix(1);
  if (input.size() > 128) return false;
  size_t count = 0;
  for (size_t i = 0; i < input.size();) {
    const auto first = static_cast<unsigned char>(input[i++]);
    uint32_t code = first;
    unsigned extra = 0;
    if (first >= 0xC2 && first <= 0xDF) { code = first & 0x1F; extra = 1; }
    else if (first >= 0xE0 && first <= 0xEF) { code = first & 0x0F; extra = 2; }
    else if (first >= 0xF0 && first <= 0xF4) { code = first & 7; extra = 3; }
    else if (first >= 0x80) return false;
    if (i + extra > input.size()) return false;
    for (unsigned n = 0; n < extra; ++n) {
      const auto next = static_cast<unsigned char>(input[i++]);
      if ((next & 0xC0) != 0x80) return false;
      code = (code << 6) | (next & 0x3F);
    }
    if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
        (extra == 3 && code < 0x10000) || code > 0x10FFFF ||
        (code >= 0xD800 && code <= 0xDFFF) || code < 0x20 ||
        (code >= 0x7F && code <= 0x9F) || ++count > 32) return false;
  }
  name.assign(input);
  return true;
}
SpeakerNameRecord encode_speaker_name(const SavedSpeakerName &value) {
  SpeakerNameRecord record{'O', 'A', 'N', 1};
  put(record, 4, value.revision);
  if (value.name.size() > 128) return {};
  const size_t length = value.name.size();
  record[8] = length;
  std::copy_n(value.name.begin(), length, record.begin() + 12);
  put(record, 140, checksum(record));
  return record;
}
bool decode_speaker_name(const SpeakerNameRecord &record, SavedSpeakerName &value) {
  if (get(record, 140) != checksum(record) || record[8] > 128 || !get(record, 4)) return false;
  SavedSpeakerName candidate{get(record, 4), std::string(record.begin() + 12, record.begin() + 12 + record[8])};
  std::string normalized;
  if (!normalize_speaker_name(candidate.name, normalized) || normalized != candidate.name ||
      encode_speaker_name(candidate) != record) return false;
  value = candidate;
  return true;
}
void SpeakerNamePreferences::begin() {
  saved_.reset(); writable_ = false;
  SavedSpeakerName value;
  const auto loaded = store_.load(value);
  if (loaded == LoadResult::ERROR) return;
  if (loaded == LoadResult::EMPTY) value = {1, ""};
  std::string normalized;
  if (!value.revision || !normalize_speaker_name(value.name, normalized) || normalized != value.name) return;
  saved_ = value; writable_ = true;
}
SpeakerNameResult SpeakerNamePreferences::update(std::string_view input, uint32_t revision) {
  if (!writable_ || !saved_) return SpeakerNameResult::STORAGE;
  std::string name;
  if (!normalize_speaker_name(input, name)) return SpeakerNameResult::INVALID;
  if (revision != saved_->revision) return SpeakerNameResult::CONFLICT;
  if (name == saved_->name) return SpeakerNameResult::UNCHANGED;
  if (revision == std::numeric_limits<uint32_t>::max()) return SpeakerNameResult::CONFLICT;
  SavedSpeakerName next{revision + 1, name};
  if (!store_.save(next)) { writable_ = false; return SpeakerNameResult::STORAGE; }
  saved_ = next;
  return SpeakerNameResult::SAVED;
}
}  // namespace openathan
