#include "openathan/display.h"
#include <limits>

namespace openathan {
namespace {
uint32_t checksum(const DisplayRecord &record) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < record.size() - 4; ++i) {
    value ^= record[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
void put(DisplayRecord &record, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) record[at + i] = value >> (8 * i);
}
uint32_t get(const DisplayRecord &record, size_t at) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(record[at + i]) << (8 * i);
  return value;
}
}
DisplayRecord encode_display(const SavedDisplay &value) {
  DisplayRecord record{'O', 'A', 'D', 1};
  put(record, 4, value.revision);
  record[8] = value.brightness_percent;
  put(record, 12, checksum(record));
  return record;
}
bool decode_display(const DisplayRecord &record, SavedDisplay &value) {
  if (get(record, 12) != checksum(record) || (record[8] < 1 || record[8] > 100) || !get(record, 4)) return false;
  SavedDisplay candidate{get(record, 4), record[8]};
  if (encode_display(candidate) != record) return false;
  value = candidate;
  return true;
}
void DisplayPreferences::begin() {
  saved_.reset(); writable_ = false;
  SavedDisplay value;
  const auto loaded = store_.load(value);
  if (loaded == LoadResult::ERROR) return;
  // Absence is a virtual revision 1. Booting does not write flash.
  if (loaded == LoadResult::EMPTY) value = {1, 10};
  if (!value.revision || (value.brightness_percent < 1 || value.brightness_percent > 100)) return;
  saved_ = value; writable_ = true;
}
DisplayResult DisplayPreferences::update(uint8_t brightness_percent, uint32_t revision) {
  if (!writable_ || !saved_) return DisplayResult::STORAGE;
  if (brightness_percent < 1 || brightness_percent > 100) return DisplayResult::INVALID;
  if (revision != saved_->revision) return DisplayResult::CONFLICT;
  if (brightness_percent == saved_->brightness_percent) return DisplayResult::UNCHANGED;
  if (revision == std::numeric_limits<uint32_t>::max()) return DisplayResult::CONFLICT;
  SavedDisplay next{revision + 1, brightness_percent};
  if (!store_.save(next)) { writable_ = false; return DisplayResult::STORAGE; }
  saved_ = next;
  return DisplayResult::SAVED;
}
}  // namespace openathan
