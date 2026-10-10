#include "openathan/lights.h"
#include <algorithm>
#include <limits>

namespace openathan {
namespace {
uint32_t checksum(const LightRecord &record) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < record.size() - 4; ++i) {
    value ^= record[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
void put(LightRecord &record, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) record[at + i] = value >> (8 * i);
}
uint32_t get(const LightRecord &record, size_t at) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(record[at + i]) << (8 * i);
  return value;
}
}
LightRecord encode_lights(const SavedLights &value) {
  LightRecord record{'O', 'A', 'L', 1};
  put(record, 4, value.revision);
  record[8] = value.value.enabled;
  record[9] = value.value.brightness_percent;
  put(record, 12, checksum(record));
  return record;
}
bool decode_lights(const LightRecord &record, SavedLights &value) {
  if (get(record, 12) != checksum(record) || record[8] > 1 || record[9] > 100 || !get(record, 4)) return false;
  SavedLights candidate{get(record, 4), {bool(record[8]), record[9]}};
  if (encode_lights(candidate) != record) return false;
  value = candidate;
  return true;
}
void LightPreferences::begin() {
  saved_.reset(); writable_ = false;
  SavedLights value;
  const auto loaded = store_.load(value);
  if (loaded == LoadResult::ERROR) return;
  // Absence is a virtual revision 1. Booting does not write flash.
  if (loaded == LoadResult::EMPTY) value = {1, {}};
  if (!value.revision || value.value.brightness_percent > 100) return;
  saved_ = value; writable_ = true;
}
LightSaveResult LightPreferences::update(LightSettings value, uint32_t revision) {
  if (!writable_ || !saved_) return LightSaveResult::STORAGE;
  if (value.brightness_percent > 100) return LightSaveResult::INVALID;
  if (revision != saved_->revision) return LightSaveResult::CONFLICT;
  if (value == saved_->value) return LightSaveResult::UNCHANGED;
  if (revision == std::numeric_limits<uint32_t>::max()) return LightSaveResult::CONFLICT;
  SavedLights next{revision + 1, value};
  if (!store_.save(next)) { writable_ = false; return LightSaveResult::STORAGE; }
  saved_ = next;
  return LightSaveResult::SAVED;
}
LightMode light_mode(const LightSettings &s, const LightInputs &i) {
  if (!s.enabled || !s.brightness_percent || i.maintenance) return LightMode::OFF;
  if (i.fault) return LightMode::FAULT;
  if (!i.setup_complete || !i.clock_ready) return LightMode::WAITING;
  if (i.playing) return LightMode::PLAYING;
  if (!i.next_utc || *i.next_utc <= i.utc) return LightMode::OFF;
  return prayer_proximity(*i.next_utc - i.utc);
}
LightMode prayer_proximity(int64_t remaining_seconds) {
  if (remaining_seconds <= 0) return LightMode::OFF;
  return remaining_seconds <= 600 ? LightMode::RED : remaining_seconds <= 1800 ? LightMode::ORANGE : LightMode::GREEN;
}
const char *light_mode_name(LightMode mode) {
  switch (mode) {
    case LightMode::GREEN: return "green";
    case LightMode::ORANGE: return "orange";
    case LightMode::RED: return "red";
    case LightMode::PLAYING: return "playing";
    case LightMode::WAITING: return "waiting";
    case LightMode::FAULT: return "fault";
    default: return "off";
  }
}
LightFrame light_frame(LightMode mode, uint8_t brightness, uint64_t ms) {
  LightFrame frame;
  frame.brightness = std::min<unsigned>(brightness, 100);
  switch (mode) {
    case LightMode::GREEN: frame.green = 255; break;
    case LightMode::ORANGE: frame.red = 255; frame.green = 96; break;
    case LightMode::RED: case LightMode::FAULT: frame.red = 255; break;
    case LightMode::PLAYING: frame.blue = 255; break;
    case LightMode::WAITING: frame.red = frame.green = frame.blue = 255; break;
    default: return {};
  }
  if (mode == LightMode::PLAYING || mode == LightMode::WAITING || mode == LightMode::FAULT) {
    // Smoothstep rise/fall, three seconds total; no floating-point animation work.
    const auto phase = ms % 3000;
    const uint32_t x = (phase <= 1500 ? phase : 3000 - phase) * 1024 / 1500;
    const uint32_t eased = x * x * (3072 - 2 * x) / (1024 * 1024);
    const uint32_t level = 26 + eased * 229 / 1024; // 10%-100% of chosen brightness.
    // Integer-percent hardware cannot represent a dimmer enabled pulse. Keep
    // low settings visible even when no frame lands exactly on the peak.
    if (frame.brightness) frame.brightness = std::max<unsigned>(1, frame.brightness * level / 255);
  }
  return frame;
}
bool LightSchedule::rebuild(DayCalculator &calculator, const Settings &settings, CivilDate date) {
  times_ = {};
  previous_isha_.reset(); following_fajr_.reset();
  if (!valid_date(date) || !valid_settings(settings)) return false;
  const auto serial = day_number(date);
  first_day_ = serial - 1;
  enabled_ = settings.enabled;
  size_t at = 0;
  // Like the scheduler, guard days classify conflicts at both window edges.
  // They never add independently selected LED/display timestamps.
  for (int offset = -2; offset <= 2; ++offset) {
    const auto day = civil_date(serial + offset);
    if (!valid_date(day)) { if (offset >= -1 && offset <= 1) at += 5; continue; }
    PrayerDay calculated;
    if (!calculator.calculate(settings, day, calculated)) { times_ = {}; return false; }
    if (offset == -2) { previous_isha_ = calculated[5]; continue; }
    if (offset == 2) { following_fajr_ = calculated[0]; continue; }
    for (unsigned index : {0U, 2U, 3U, 4U, 5U}) times_[at++] = calculated[index];
  }
  return true;
}
std::optional<int64_t> LightSchedule::next(int64_t utc) const {
  std::optional<int64_t> result;
  for (const auto &time : times_) if (time && *time > utc && (!result || *time < *result)) result = time;
  return result;
}
std::optional<Event> LightSchedule::next_event(int64_t utc) const {
  size_t at = times_.size();
  for (size_t i = 0; i < times_.size(); ++i)
    if (times_[i] && *times_[i] > utc && (at == times_.size() || *times_[i] < *times_[at])) at = i;
  if (at == times_.size()) return {};
  const auto key = [this](size_t i) { return EventKey{first_day_ + static_cast<int32_t>(i / 5), static_cast<Prayer>(i % 5)}; };
  Event event{key(at), *times_[at], enabled_[at % 5], {}};
  // Match the scheduler's shared Isha/Fajr identity without filtering muted
  // prayers or consulting consumption history. The LED timestamp stays raw.
  const auto share = [&](EventKey isha, EventKey fajr) {
    event.key = fajr; event.shared_with = isha;
    if (!enabled_[0] && enabled_[4]) std::swap(event.key, *event.shared_with);
    event.enabled = enabled_[0] || enabled_[4];
  };
  if (at % 5 == 0) {
    const auto previous = at ? times_[at - 1] : previous_isha_;
    if (previous == times_[at]) share({event.key.day - 1, Prayer::ISHA}, event.key);
  } else if (at % 5 == 4) {
    const auto following = at + 1 < times_.size() ? times_[at + 1] : following_fajr_;
    if (following == times_[at]) {
      share(event.key, {event.key.day + 1, Prayer::FAJR});
    } else if (following && *times_[at] > *following) {
      event.enabled = false;  // The scheduler suppresses a late overlapping Isha.
    }
  }
  return event;
}
}  // namespace openathan
