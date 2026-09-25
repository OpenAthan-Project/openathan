#include "local_api.h"

namespace esphome::openathan_device {
namespace {
void error(ApiExchange& request, int code, const char* message) {
  request.code = code;
  JsonDocument doc;
  doc["error"] = message;
  request.response.clear();
  serializeJson(doc, request.response);
}
void key_json(JsonObject root, const ::openathan::EventKey& key) {
  root["day"] = key.day;
  root["prayer"] = unsigned(key.prayer);
}
void event_json(JsonObject root, const ::openathan::Event& event) {
  key_json(root, event.key);
  root["utc"] = event.utc;
  if (event.shared_with) key_json(root["shared_with"].to<JsonObject>(), *event.shared_with);
}
bool parse_key(JsonObjectConst root, ::openathan::EventKey& key) {
  if (!root["day"].is<int32_t>() || !root["prayer"].is<unsigned>() || root["prayer"].as<unsigned>() >= 5) return false;
  key = {root["day"].as<int32_t>(), static_cast<::openathan::Prayer>(root["prayer"].as<unsigned>())};
  return true;
}
}  // namespace
void LocalApi::preview_(JsonObject root, const ::openathan::DeviceSettings& settings) {
  root["clock_ready"] = athan_->read().valid;
  if (!athan_->read().valid) {
    root["state"] = "waiting_for_time";
    return;
  }
  ::openathan::PrayerDay times;
  std::vector<::openathan::Event> events;
  std::vector<::openathan::ScheduleConflict> conflicts;
  if (!athan_->preview(settings, times, events, conflicts)) {
    root["state"] = "invalid_schedule";
    return;
  }
  root["state"] = "ready";
  auto day = root["times"].to<JsonArray>();
  constexpr const char* names[] = {"Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"};
  for (unsigned i = 0; i < 6; ++i) {
    auto row = day.add<JsonObject>();
    row["name"] = names[i];
    if (times[i]) {
      row["utc"] = *times[i];
      row["local"] = athan_->format_local(*times[i], settings.timezone);
    } else
      row["local"] = "Unavailable";
  }
  auto notices = root["conflicts"].to<JsonArray>();
  for (const auto& conflict : conflicts) notices.add(::openathan::describe_conflict(conflict));
}
void LocalApi::snapshot_(JsonObject root) {
  root["test_mode"] = openathan_storage::TEST_MODE;
  athan_->write_settings_json(root);
  root["setup"] = athan_->setup_state();
  root["clock_ready"] = athan_->read().valid;
  root["wifi_connected"] = wifi_connected_;
  root["hostname"] = hostname_;
  const auto status = athan_->status();
  root["playing"] = status.playing;
  const auto* service = athan_->settings_service();
  if (status.next) {
    auto next = root["next"].to<JsonObject>();
    event_json(next, *status.next);
    next["name"] = ::openathan::prayer_name(status.next->key.prayer);
    if (service && service->saved())
      next["local"] = athan_->format_local(status.next->utc, service->saved()->value.timezone);
  }
  if (status.skip) key_json(root["skip"].to<JsonObject>(), *status.skip);
  if (service && service->saved() && athan_->activated())
    preview_(root["schedule"].to<JsonObject>(), service->saved()->value);
  else
    root["schedule"]["state"] = "setup_required";
}
bool LocalApi::resolve_timezone_(JsonObject settings) {
  if (!settings["timezone"].is<const char*>()) return false;
  const auto name = settings["timezone"].as<std::string>();
  for (size_t i = 0; i < zone_count_; ++i)
    if (name == zones_[i].name) {
      JsonDocument rules;
      if (deserializeJson(rules, zones_[i].rules)) return false;
      settings["timezone_rules"].set(rules.as<JsonObject>());
      return true;
    }
  return false;
}
void LocalApi::handle(ApiExchange& request) {
  JsonDocument output;
  auto root = output.to<JsonObject>();
  if (request.method == "GET" && request.uri == "/api/status")
    snapshot_(root);
  else if (request.method == "GET" && request.uri == "/api/timezones") {
    root["tzdata"] = "2026.4";
    auto names = root["names"].to<JsonArray>();
    for (size_t i = 0; i < zone_count_; ++i) names.add(zones_[i].name);
  } else if (request.method == "POST") {
    JsonDocument input;
    if (deserializeJson(input, request.body) || !input.is<JsonObject>()) {
      error(request, 400, "Invalid JSON object");
      return;
    }
    auto payload = input.as<JsonObject>();
    const bool settings_action =
        request.uri == "/api/settings" || request.uri == "/api/preview" || request.uri == "/api/activate";
    if (settings_action) {
      if (!payload["refresh_timezone"].isUnbound() && !payload["refresh_timezone"].is<bool>()) {
        error(request, 400, "Invalid timezone refresh");
        return;
      }
      const bool refresh = payload["refresh_timezone"] | false;
      payload.remove("refresh_timezone");
      if (!payload["settings"].is<JsonObject>()) {
        error(request, 400, "Settings are required");
        return;
      }
      auto settings = payload["settings"].as<JsonObject>();
      const auto* service = athan_->settings_service();
      if (!service || !service->healthy() || !service->saved()) {
        error(request, 503, "Settings storage unavailable");
        return;
      }
      if (settings["timezone"].is<const char*>() &&
          settings["timezone"].as<std::string>() == service->saved()->value.timezone.name && !refresh) {
        JsonDocument current;
        athan_->write_settings_json(current.to<JsonObject>());
        settings["timezone_rules"].set(current["settings"]["timezone_rules"]);
      } else if (!resolve_timezone_(settings)) {
        error(request, 400, "Unsupported timezone");
        return;
      }
      ::openathan::DeviceSettings candidate;
      uint32_t revision;
      if (!athan_->parse_settings_json(request.body, payload, candidate, revision)) {
        error(request, 400, "Invalid settings document");
        return;
      }
      if (revision != service->saved()->revision) {
        error(request, 409, "Settings changed on another client; reload before saving");
        return;
      }
      if (request.uri == "/api/preview")
        preview_(root, candidate);
      else {
        if (request.uri == "/api/activate" && athan_->read().valid) {
          ::openathan::PrayerDay day;
          std::vector<::openathan::Event> events;
          std::vector<::openathan::ScheduleConflict> conflicts;
          if (!athan_->preview(candidate, day, events, conflicts)) {
            error(request, 400, "Invalid prayer schedule; review settings before activation");
            return;
          }
        }
        const auto result = athan_->change_settings(candidate, revision);
        if (result != ::openathan::SettingsResult::SAVED && result != ::openathan::SettingsResult::UNCHANGED) {
          error(request, result == ::openathan::SettingsResult::STORAGE ? 503 : 400,
                ::openathan::settings_result_name(result));
          return;
        }
        if (request.uri == "/api/activate" && !athan_->finish_setup(service->saved()->revision)) {
          error(request, 503, "Activation incomplete; read device state before retrying");
          return;
        }
        snapshot_(root);
      }
    } else if (request.uri == "/api/stop" && payload.size() == 0) {
      athan_->stop();
      snapshot_(root);
    } else if (request.uri == "/api/skip" || request.uri == "/api/cancel-skip") {
      const auto* service = athan_->settings_service();
      if (payload.size() != 2 || !payload["expected_revision"].is<uint32_t>() ||
          !payload["occurrence"].is<JsonObject>() || !service || !service->saved() ||
          payload["expected_revision"].as<uint32_t>() != service->saved()->revision) {
        error(request, 409, "Reload the displayed occurrence");
        return;
      }
      const auto occurrence = payload["occurrence"].as<JsonObjectConst>();
      ::openathan::Event expected;
      if (!parse_key(occurrence, expected.key)) {
        error(request, 400, "Invalid occurrence");
        return;
      }
      bool ok = false;
      if (request.uri == "/api/skip") {
        if (!occurrence["utc"].is<int64_t>()) {
          error(request, 400, "Occurrence time required");
          return;
        }
        expected.utc = occurrence["utc"].as<int64_t>();
        if (!occurrence["shared_with"].isUnbound()) {
          ::openathan::EventKey shared;
          if (!parse_key(occurrence["shared_with"], shared)) {
            error(request, 400, "Invalid shared occurrence");
            return;
          }
          expected.shared_with = shared;
        }
        ok = athan_->skip_occurrence(expected);
      } else
        ok = athan_->cancel_occurrence(expected.key);
      if (!ok) {
        error(request, 409, "Occurrence changed or device not ready; reload status");
        return;
      }
      snapshot_(root);
    } else {
      error(request, 404, "Unknown endpoint");
      return;
    }
  } else {
    error(request, 404, "Unknown endpoint");
    return;
  }
  serializeJson(output, request.response);
}
}  // namespace esphome::openathan_device
