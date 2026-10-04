#include "openathan/time_format.h"
#include <cstdio>
#include <limits>

namespace openathan {
namespace {
uint32_t checksum(const TimeFormatRecord &record) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < record.size() - 4; ++i) {
    value ^= record[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
void put(TimeFormatRecord &record, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) record[at + i] = value >> (8 * i);
}
uint32_t get(const TimeFormatRecord &record, size_t at) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(record[at + i]) << (8 * i);
  return value;
}
}
TimeFormatRecord encode_time_format(const SavedTimeFormat &value) {
  TimeFormatRecord record{'O', 'A', 'T', 1};
  put(record, 4, value.revision);
  record[8] = value.hours;
  put(record, 12, checksum(record));
  return record;
}
bool decode_time_format(const TimeFormatRecord &record, SavedTimeFormat &value) {
  if (get(record, 12) != checksum(record) || (record[8] != 12 && record[8] != 24) || !get(record, 4)) return false;
  SavedTimeFormat candidate{get(record, 4), record[8]};
  if (encode_time_format(candidate) != record) return false;
  value = candidate;
  return true;
}
void TimeFormatPreferences::begin() {
  saved_.reset(); writable_ = false;
  SavedTimeFormat value;
  const auto loaded = store_.load(value);
  if (loaded == LoadResult::ERROR) return;
  // Absence is a virtual revision 1. Booting does not write flash.
  if (loaded == LoadResult::EMPTY) value = {1, 24};
  if (!value.revision || (value.hours != 12 && value.hours != 24)) return;
  saved_ = value; writable_ = true;
}
TimeFormatResult TimeFormatPreferences::update(uint8_t hours, uint32_t revision) {
  if (!writable_ || !saved_) return TimeFormatResult::STORAGE;
  if (hours != 12 && hours != 24) return TimeFormatResult::INVALID;
  if (revision != saved_->revision) return TimeFormatResult::CONFLICT;
  if (hours == saved_->hours) return TimeFormatResult::UNCHANGED;
  if (revision == std::numeric_limits<uint32_t>::max()) return TimeFormatResult::CONFLICT;
  SavedTimeFormat next{revision + 1, hours};
  if (!store_.save(next)) { writable_ = false; return TimeFormatResult::STORAGE; }
  saved_ = next;
  return TimeFormatResult::SAVED;
}
std::array<char, 9> format_clock(std::string_view value, uint8_t hours) {
  std::array<char, 9> out{};
  if (value.size() != 5 || value[2] != ':' || value[0] < '0' || value[0] > '2' ||
      value[1] < '0' || value[1] > '9' || (value[0] == '2' && value[1] > '3') ||
      value[3] < '0' || value[3] > '5' || value[4] < '0' || value[4] > '9') return out;
  const unsigned hour = (value[0] - '0') * 10 + value[1] - '0';
  const unsigned minute = (value[3] - '0') * 10 + value[4] - '0';
  if (hours == 12) std::snprintf(out.data(), out.size(), "%u:%02u %s", hour % 12 ? hour % 12 : 12,
                               minute, hour < 12 ? "AM" : "PM");
  else std::snprintf(out.data(), out.size(), "%02u:%02u", hour, minute);
  return out;
}
}  // namespace openathan
