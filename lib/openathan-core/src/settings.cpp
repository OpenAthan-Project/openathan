#include "openathan/settings.h"
#include <bit>
#include <cstring>
#include <limits>

namespace openathan {
static bool valid_rule(const DstRule &r) {
  if (r.time_seconds < -167*3600-3599 || r.time_seconds > 167*3600+3599) return false;
  switch (r.type) {
    case DstRuleType::NONE: return r == DstRule{};
    case DstRuleType::MONTH_WEEK_DAY:
      return r.day == 0 && r.month >= 1 && r.month <= 12 && r.week >= 1 && r.week <= 5 && r.day_of_week <= 6;
    case DstRuleType::JULIAN_NO_LEAP:
      return r.day >= 1 && r.day <= 365 && !r.month && !r.week && !r.day_of_week;
    case DstRuleType::DAY_OF_YEAR:
      return r.day <= 365 && !r.month && !r.week && !r.day_of_week;
  }
  return false;
}
bool valid_timezone(const Timezone &tz) {
  if (tz.name.empty() || tz.name.size() > TIMEZONE_NAME_SIZE) return false;
  for (unsigned char c : tz.name) if (c < 33 || c > 126) return false;
  if (tz.standard_offset < -86400 || tz.standard_offset > 86400 ||
      tz.daylight_offset < -86400 || tz.daylight_offset > 86400) return false;
  if (!valid_rule(tz.start) || !valid_rule(tz.end)) return false;
  const bool no_dst = tz.start.type == DstRuleType::NONE;
  return no_dst == (tz.end.type == DstRuleType::NONE) && (!no_dst || tz.daylight_offset == 0);
}
bool valid_device_settings(const DeviceSettings &s) {
  return valid_settings(s.prayer) && valid_timezone(s.timezone) && s.volume <= 100;
}
static uint32_t crc(const SettingsRecord &record) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < record.size()-4; ++i) {
    value ^= record[i];
    for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
SettingsRecord encode_settings(const SavedSettings &saved) {
  SettingsRecord out{};
  size_t at = 0;
  const auto put = [&](uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out[at++] = static_cast<uint8_t>(value >> (8*i));
  };
  for (char c : std::string("OAC1")) put(c, 1);
  put(saved.revision, 4);
  const auto &s = saved.value;
  put(std::bit_cast<uint64_t>(s.prayer.latitude), 8);
  put(std::bit_cast<uint64_t>(s.prayer.longitude), 8);
  put(static_cast<unsigned>(s.prayer.method), 1); put(s.prayer.hanafi, 1);
  put(static_cast<unsigned>(s.prayer.high_latitude), 1);
  for (int offset : s.prayer.offsets) put(static_cast<uint16_t>(offset), 2);
  unsigned enabled = 0;
  for (unsigned i = 0; i < 5; ++i) if (s.prayer.enabled[i]) enabled |= 1U << i;
  put(enabled, 1); put(s.volume, 1); put(s.timezone.name.size(), 1);
  for (size_t i = 0; i < TIMEZONE_NAME_SIZE; ++i) put(i < s.timezone.name.size() ? s.timezone.name[i] : 0, 1);
  put(static_cast<uint32_t>(s.timezone.standard_offset), 4);
  put(static_cast<uint32_t>(s.timezone.daylight_offset), 4);
  for (const auto &r : {s.timezone.start, s.timezone.end}) {
    put(static_cast<uint32_t>(r.time_seconds), 4); put(r.day, 2);
    put(static_cast<unsigned>(r.type), 1); put(r.month, 1); put(r.week, 1); put(r.day_of_week, 1);
  }
  at = out.size()-4; put(crc(out), 4);
  return out;
}
bool decode_settings(const SettingsRecord &record, SavedSettings &out) {
  size_t at = record.size()-4;
  const auto get = [&](unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= uint64_t(record[at++]) << (8*i);
    return value;
  };
  if (get(4) != crc(record) || std::memcmp(record.data(), "OAC1", 4)) return false;
  SavedSettings candidate;
  at = 4; candidate.revision = get(4);
  auto &s = candidate.value;
  s.prayer.latitude = std::bit_cast<double>(get(8)); s.prayer.longitude = std::bit_cast<double>(get(8));
  s.prayer.method = static_cast<Method>(get(1));
  const auto hanafi = get(1); if (hanafi > 1) return false; s.prayer.hanafi = hanafi;
  s.prayer.high_latitude = static_cast<HighLatitudeRule>(get(1));
  for (auto &offset : s.prayer.offsets) offset = std::bit_cast<int16_t>(static_cast<uint16_t>(get(2)));
  const auto enabled = get(1); if (enabled > 31) return false;
  for (unsigned i = 0; i < 5; ++i) s.prayer.enabled[i] = enabled & (1U << i);
  s.volume = get(1);
  const size_t length = get(1); if (length > TIMEZONE_NAME_SIZE) return false;
  s.timezone.name.assign(reinterpret_cast<const char *>(record.data()+at), length); at += TIMEZONE_NAME_SIZE;
  s.timezone.standard_offset = std::bit_cast<int32_t>(static_cast<uint32_t>(get(4)));
  s.timezone.daylight_offset = std::bit_cast<int32_t>(static_cast<uint32_t>(get(4)));
  for (auto *r : {&s.timezone.start, &s.timezone.end}) {
    r->time_seconds = std::bit_cast<int32_t>(static_cast<uint32_t>(get(4))); r->day = get(2);
    r->type = static_cast<DstRuleType>(get(1)); r->month = get(1); r->week = get(1); r->day_of_week = get(1);
  }
  if (!candidate.revision || !valid_device_settings(s) || encode_settings(candidate) != record) return false;
  out = std::move(candidate);
  return true;
}
const char *settings_result_name(SettingsResult result) {
  switch (result) {
    case SettingsResult::SAVED: return "saved";
    case SettingsResult::UNCHANGED: return "unchanged";
    case SettingsResult::CONFLICT: return "revision conflict";
    case SettingsResult::INVALID: return "invalid settings";
    case SettingsResult::INVALID_SCHEDULE: return "invalid prayer schedule";
    case SettingsResult::STORAGE: return "settings storage unavailable; restart required";
  }
  return "unknown";
}
bool SettingsService::begin(const DeviceSettings &defaults) {
  healthy_ = false; saved_.reset();
  SavedSettings loaded;
  const auto result = store_.load(loaded);
  if (result == LoadResult::ERROR) return false;
  if (result == LoadResult::EMPTY) {
    if (!valid_device_settings(defaults)) return false;
    loaded = {1, defaults};
    if (!store_.save(loaded)) return false;
  }
  if (!loaded.revision || !valid_device_settings(loaded.value)) return false;
  saved_ = std::move(loaded); healthy_ = true;
  return true;
}
SettingsResult SettingsService::update(const DeviceSettings &candidate, uint32_t expected,
                                      const std::function<bool(const DeviceSettings &)> &preflight) {
  if (!healthy_ || !saved_) return SettingsResult::STORAGE;
  if (expected != saved_->revision) return SettingsResult::CONFLICT;
  if (!valid_device_settings(candidate)) return SettingsResult::INVALID;
  if (candidate == saved_->value) return SettingsResult::UNCHANGED;
  if (!preflight(candidate)) return SettingsResult::INVALID_SCHEDULE;
  if (saved_->revision == std::numeric_limits<uint32_t>::max()) return SettingsResult::CONFLICT;
  const SavedSettings next{saved_->revision+1, candidate};
  if (!store_.save(next)) { healthy_ = false; return SettingsResult::STORAGE; }
  saved_ = next;
  return SettingsResult::SAVED;
}
}  // namespace openathan
