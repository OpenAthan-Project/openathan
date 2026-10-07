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
void LocalApi::time_format_(JsonObject root) {
  const auto &preferences = athan_->time_format_preferences();
  const auto &saved = preferences.saved();
  root["schema"] = 1;
  root["revision"] = saved ? saved->revision : 0;
  root["hours"] = preferences.hours();
  root["application"] = preferences.writable() ? "applied" : saved ? "save_failed" : "storage_fault";
}
void LocalApi::display_(JsonObject root) {
  const auto &preferences = athan_->display_preferences();
  root["schema"] = 1;
  root["supported"] = athan_->has_display();
  root["revision"] = preferences.saved() ? preferences.saved()->revision : 0;
  root["brightness_percent"] = preferences.brightness_percent();
  root["application"] = athan_->display_application_status();
}
void LocalApi::lights_(JsonObject root) {
  root["supported"] = athan_->has_lights();
  root["application"] = athan_->light_application_status();
  root["mode"] = ::openathan::light_mode_name(athan_->current_light_mode());
  root["schema"] = 1;
  const auto &saved = athan_->light_preferences().saved();
  root["revision"] = saved ? saved->revision : 0;
  if (saved) {
    root["settings"]["enabled"] = saved->value.enabled;
    root["settings"]["brightness_percent"] = saved->value.brightness_percent;
  }
}
void LocalApi::snapshot_(JsonObject root) {
  if (upgrade_) upgrade_->snapshot(root["firmware"].to<JsonObject>());
  time_format_(root["time_format"].to<JsonObject>());
  lights_(root["lights"].to<JsonObject>());
  display_(root["display"].to<JsonObject>());
  root["test_mode"] = openathan_storage::TEST_MODE;
  athan_->write_settings_json(root);
  root["setup"] = athan_->setup_state();
  const auto now = athan_->read();
  root["clock_ready"] = now.valid;
  const auto* service = athan_->settings_service();
  if (now.valid && service && service->saved()) {
    const auto local = athan_->format_local(now.utc, service->saved()->value.timezone);
    if (local.size() >= 10) root["local_date"] = local.substr(0, 10);
  }
  root["wifi_connected"] = wifi_connected_;
  root["hostname"] = hostname_;
  const auto status = athan_->status();
  root["playing"] = status.playing;
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
  else if (request.method == "GET" && request.uri == "/api/firmware" && upgrade_)
    upgrade_->snapshot(root);
  else if (request.method == "GET" && request.uri == "/api/time-format")
    time_format_(root);
  else if (request.method == "GET" && request.uri == "/api/display")
    display_(root);
  else if (request.method == "GET" && request.uri == "/api/lights")
    lights_(root);
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
    if (request.uri.rfind("/api/firmware/", 0) == 0 && upgrade_) {
      std::string message;
      const int code = upgrade_->action(request.uri.substr(14), payload, message);
      if (code != 200) { error(request, code, message.c_str()); return; }
      upgrade_->snapshot(root);
    } else if (request.uri == "/api/time-format") {
      if (payload.size() != 3 || !payload["schema"].is<unsigned>() || payload["schema"].as<unsigned>() != 1 ||
          !payload["expected_revision"].is<uint32_t>() || !payload["hours"].is<unsigned>() ||
          (payload["hours"].as<unsigned>() != 12 && payload["hours"].as<unsigned>() != 24)) {
        error(request, 400, "Choose 12-hour or 24-hour time"); return;
      }
      const auto result = athan_->change_time_format(payload["hours"].as<uint8_t>(), payload["expected_revision"].as<uint32_t>());
      using ::openathan::TimeFormatResult;
      if (result == TimeFormatResult::CONFLICT) { error(request, 409, "Reload time format before saving"); return; }
      if (result == TimeFormatResult::STORAGE) { error(request, 503, "Time format could not be saved; restart the device"); return; }
      time_format_(root);
    } else if (request.uri == "/api/display") {
      if (!athan_->has_display()) { error(request, 404, "Screen brightness is not supported on this device"); return; }
      if (payload.size() != 3 || !payload["schema"].is<unsigned>() || payload["schema"].as<unsigned>() != 1 ||
          !payload["expected_revision"].is<uint32_t>() || !payload["brightness_percent"].is<unsigned>() ||
          payload["brightness_percent"].as<unsigned>() < 1 || payload["brightness_percent"].as<unsigned>() > 100) {
        error(request, 400, "Choose screen brightness from 1 to 100 percent"); return;
      }
      const auto result = athan_->change_display(payload["brightness_percent"].as<uint8_t>(), payload["expected_revision"].as<uint32_t>());
      using ::openathan::DisplayResult;
      if (result == DisplayResult::CONFLICT) { error(request, 409, "Reload screen brightness before saving"); return; }
      if (result == DisplayResult::STORAGE) { error(request, 503, "Screen brightness could not be saved; restart the device"); return; }
      display_(root);
    } else if (request.uri == "/api/lights") {
      if (!athan_->has_lights()) { error(request, 404, "Lights are not supported on this device"); return; }
      const auto settings = payload["settings"];
      if (payload.size() != 3 || !payload["schema"].is<unsigned>() || payload["schema"].as<unsigned>() != 1 ||
          !payload["expected_revision"].is<uint32_t>() || !settings.is<JsonObject>() || settings.size() != 2 ||
          !settings["enabled"].is<bool>() || !settings["brightness_percent"].is<unsigned>() ||
          settings["brightness_percent"].as<unsigned>() > 100) {
        error(request, 400, "Invalid light settings"); return;
      }
      const auto result = athan_->change_lights({settings["enabled"].as<bool>(), settings["brightness_percent"].as<uint8_t>()},
          payload["expected_revision"].as<uint32_t>());
      using ::openathan::LightSaveResult;
      if (result == LightSaveResult::CONFLICT) { error(request, 409, "Reload saved light settings before saving"); return; }
      if (result == LightSaveResult::STORAGE) { error(request, 503, "Light settings could not be saved; restart the device"); return; }
      if (result == LightSaveResult::INVALID) { error(request, 400, "Invalid light settings"); return; }
      lights_(root);
    } else if (settings_action) {
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
