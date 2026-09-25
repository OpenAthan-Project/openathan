#include "openathan.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

namespace esphome::openathan_component {
namespace {
constexpr const char *METHODS[] = {"muslim_world_league", "egyptian", "karachi", "umm_al_qura", "dubai",
    "moonsighting_committee", "north_america", "kuwait", "qatar", "singapore", "tehran", "turkey"};
constexpr const char *HIGH_LATITUDE[] = {"middle_of_night", "seventh_of_night", "twilight_angle", "auto"};
constexpr const char *EVENTS[] = {"fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"};
constexpr const char *PRAYERS[] = {"fajr", "dhuhr", "asr", "maghrib", "isha"};
std::string coordinate_number(double value) {
  // ArduinoJson's default writer limits significant digits. Keep these as
  // JSON numbers while retaining the exact stored double for export/readback.
  char number[32];
  std::snprintf(number, sizeof(number), "%.*g", std::numeric_limits<double>::max_digits10, value);
  return number;
}
bool read_coordinates(const std::string &payload, ::openathan::Settings &settings) {
  // The typed document has already been checked. Parse a second view with
  // numeric tokens quoted so ArduinoJson cannot round them through float or
  // its limited-mantissa number parser. It still handles structure, escaped
  // keys and duplicate-key semantics; strtod handles only the two coordinates.
  std::string quoted;
  quoted.reserve(payload.size());
  bool in_string = false;
  for (size_t i = 0; i < payload.size();) {
    const char c = payload[i];
    if (!in_string && (c == '-' || (c >= '0' && c <= '9'))) {
      const auto end = payload.find_first_not_of("0123456789.eE+-", i);
      const auto count = (end == std::string::npos ? payload.size() : end) - i;
      quoted += '"'; quoted.append(payload, i, count); quoted += '"'; i += count;
    } else {
      quoted += c; ++i;
      if (in_string && c == '\\' && i < payload.size()) quoted += payload[i++];
      else if (c == '"') in_string = !in_string;
    }
  }
  return json::parse_json(quoted, [&settings](JsonObject root) {
    for (const auto &[name, value] : {std::pair{"latitude", &settings.latitude},
                                    std::pair{"longitude", &settings.longitude}}) {
      if (!root["settings"][name].is<const char *>()) return false;
      const auto number = root["settings"][name].as<std::string>();
      char *end;
      *value = std::strtod(number.c_str(), &end);
      if (end != number.c_str() + number.size() || !std::isfinite(*value)) return false;
    }
    return true;
  });
}
template<size_t N> bool choice(JsonVariantConst value, const char *const (&names)[N], unsigned &out) {
  if (!value.is<const char *>()) return false;
  const auto string = value.as<JsonString>();
  for (unsigned i = 0; i < N; ++i)
    if (string.size() == std::strlen(names[i]) && std::memcmp(string.c_str(), names[i], string.size()) == 0) {
      out = i; return true;
    }
  return false;
}
bool read_rule(JsonVariantConst value, ::openathan::DstRule &out) {
  if (!value.is<JsonObjectConst>() || value.size() != 6 || !value["type"].is<uint8_t>() ||
      !value["time_seconds"].is<int32_t>() || !value["day"].is<uint16_t>() || !value["month"].is<uint8_t>() ||
      !value["week"].is<uint8_t>() || !value["day_of_week"].is<uint8_t>()) return false;
  out = {value["time_seconds"].as<int32_t>(), value["day"].as<uint16_t>(),
      static_cast<::openathan::DstRuleType>(value["type"].as<uint8_t>()), value["month"].as<uint8_t>(),
      value["week"].as<uint8_t>(), value["day_of_week"].as<uint8_t>()};
  return true;
}
void write_rule(JsonObject root, const ::openathan::DstRule &r) {
  root["type"] = static_cast<unsigned>(r.type); root["time_seconds"] = r.time_seconds;
  root["day"] = r.day; root["month"] = r.month; root["week"] = r.week; root["day_of_week"] = r.day_of_week;
}
bool read_value(JsonVariantConst root, ::openathan::DeviceSettings &s) {
  if (!root.is<JsonObjectConst>() || root.size() != 10 || !root["latitude"].is<double>() ||
      !root["longitude"].is<double>() || !root["volume"].is<uint8_t>() || !root["timezone"].is<const char *>()) return false;
  s.prayer.latitude = root["latitude"].as<double>(); s.prayer.longitude = root["longitude"].as<double>();
  s.volume = root["volume"].as<uint8_t>();
  const auto name = root["timezone"].as<std::string>();
  if (name.size() > ::openathan::TIMEZONE_NAME_SIZE) return false;
  s.timezone.name = name;
  unsigned selected;
  if (!choice(root["method"], METHODS, selected)) return false;
  s.prayer.method = static_cast<::openathan::Method>(selected);
  if (!choice(root["high_latitude"], HIGH_LATITUDE, selected)) return false;
  s.prayer.high_latitude = static_cast<::openathan::HighLatitudeRule>(selected);
  constexpr const char *ASR[] = {"standard", "hanafi"};
  if (!choice(root["asr_method"], ASR, selected)) return false;
  s.prayer.hanafi = selected == 1;
  const auto offsets = root["offsets"], enabled = root["enabled"], tz = root["timezone_rules"];
  if (!offsets.is<JsonObjectConst>() || offsets.size() != 6 || !enabled.is<JsonObjectConst>() || enabled.size() != 5 ||
      !tz.is<JsonObjectConst>() || tz.size() != 4 || !tz["standard_offset"].is<int32_t>() ||
      !tz["daylight_offset"].is<int32_t>()) return false;
  for (unsigned i = 0; i < 6; ++i) {
    if (!offsets[EVENTS[i]].is<int>()) return false;
    s.prayer.offsets[i] = offsets[EVENTS[i]].as<int>();
  }
  for (unsigned i = 0; i < 5; ++i) {
    if (!enabled[PRAYERS[i]].is<bool>()) return false;
    s.prayer.enabled[i] = enabled[PRAYERS[i]].as<bool>();
  }
  s.timezone.standard_offset = tz["standard_offset"].as<int32_t>();
  s.timezone.daylight_offset = tz["daylight_offset"].as<int32_t>();
  return read_rule(tz["start"], s.timezone.start) && read_rule(tz["end"], s.timezone.end);
}
}  // namespace
void OpenAthan::write_settings_json(JsonObject root) const {
  root["schema"] = 1;
  root["application"] = settings_application_status();
  root["scheduler_fault"] = ::openathan::fault_name(status().fault);
  root["automatic_ready"] = status().automatic_ready;
  if (scheduler_) {
    auto consumed = root["consumed_through"].to<JsonObject>();
    for (unsigned i = 0; i < 5; ++i) consumed[PRAYERS[i]] = scheduler_->consumed_through()[i];
  }
  if (!settings_service_ || !settings_service_->saved()) { root["revision"] = 0; return; }
  const auto &saved = *settings_service_->saved();
  root["revision"] = saved.revision;
  auto value = root["settings"].to<JsonObject>();
  const auto &s = saved.value;
  value["latitude"] = serialized(coordinate_number(s.prayer.latitude));
  value["longitude"] = serialized(coordinate_number(s.prayer.longitude));
  value["method"] = METHODS[static_cast<unsigned>(s.prayer.method)];
  value["asr_method"] = s.prayer.hanafi ? "hanafi" : "standard";
  value["high_latitude"] = HIGH_LATITUDE[static_cast<unsigned>(s.prayer.high_latitude)];
  value["volume"] = s.volume; value["timezone"] = s.timezone.name;
  auto offsets = value["offsets"].to<JsonObject>();
  for (unsigned i = 0; i < 6; ++i) offsets[EVENTS[i]] = s.prayer.offsets[i];
  auto enabled = value["enabled"].to<JsonObject>();
  for (unsigned i = 0; i < 5; ++i) enabled[PRAYERS[i]] = s.prayer.enabled[i];
  auto tz = value["timezone_rules"].to<JsonObject>();
  tz["standard_offset"] = s.timezone.standard_offset; tz["daylight_offset"] = s.timezone.daylight_offset;
  write_rule(tz["start"].to<JsonObject>(), s.timezone.start);
  write_rule(tz["end"].to<JsonObject>(), s.timezone.end);
}
void OpenAthan::read_settings_json(const std::string &payload) {
  request_ok_ = false;
  request_error_ = "invalid settings document";
  if (payload.size() > 4096) return;
  json::parse_json(payload, [this, &payload](JsonObject root) {
    ::openathan::DeviceSettings candidate;
    if (root.size() != 3 || !root["schema"].is<unsigned>() || root["schema"].as<unsigned>() != 1 ||
        !root["expected_revision"].is<uint32_t>() || !read_value(root["settings"], candidate) ||
        !read_coordinates(payload, candidate.prayer) || !::openathan::valid_device_settings(candidate)) return false;
    const auto result = change_settings(candidate, root["expected_revision"].as<uint32_t>());
    request_ok_ = result == ::openathan::SettingsResult::SAVED || result == ::openathan::SettingsResult::UNCHANGED;
    request_error_ = request_ok_ ? "" : ::openathan::settings_result_name(result);
    return true;
  });
}
}  // namespace esphome::openathan_component
