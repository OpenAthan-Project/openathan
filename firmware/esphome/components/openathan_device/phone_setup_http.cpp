#include "device.h"
#include <esp_timer.h>
#include <algorithm>

namespace esphome::openathan_device {
namespace {
void fail(HttpExchange &request, int status, const char *message) {
  request.code = status;
  JsonDocument document;
  document["error"] = message;
  serializeJson(document, request.response);
}
struct ClearBody {
  std::string &body;
  ~ClearBody() { std::fill(body.begin(), body.end(), '\0'); body.clear(); }
};
}
bool Device::handle_phone_http_(HttpExchange &request) {
  // The wildcard HTTP listener cannot prove the ingress netif. First-run
  // bootstrap therefore admits no requests while any STA connection exists.
  const auto *wifi = wifi::global_wifi_component;
  const bool on_ap = wifi && wifi->is_ap_active() && !wifi_attempt_.active() && !connected() &&
      phone_setup::ap_socket(phone_setup_active(), request.local_ip, ipv4_address());
  if (!on_ap) {
    if (!phone_setup::setup_path(request.uri)) return false;
    ClearBody clear_body{request.body};
    fail(request, 403, "Join the speaker's setup Wi-Fi to continue");
    return true;
  }
  ClearBody clear_body{request.body};
  // A queued request must still belong to the live first-run window when it
  // reaches the main loop. A previous association or Host alone is insufficient.
  if (!phone_eligible_() && phone_state_ != phone_setup::State::STORAGE_FAULT) {
    fail(request, 403, "Phone setup is closed. Use website USB recovery if needed");
    return true;
  }
  if (request.method == "GET" && (!phone_setup::canonical_host(request.host) || !phone_setup::setup_path(request.uri))) {
    request.code = 302; request.redirect = std::string(phone_setup::URL); request.type = "text/plain";
    request.response = "Open the speaker setup page";
    return true;
  }
  if (!phone_setup::canonical_host(request.host) ||
      (!request.origin.empty() && request.origin != "http://" + request.host)) {
    fail(request, 403, "Use the speaker's setup address"); return true;
  }
  if (request.method == "GET") {
    if (request.uri == "/setup") {
      request.code = 302; request.redirect = std::string(phone_setup::URL); return true;
    }
    constexpr const char *paths[]{"/setup/", "/setup/style.css", "/setup/app.js"};
    for (unsigned i = 0; i < 3; ++i) if (request.uri == paths[i]) {
      const auto &asset = assets_[i + 3];
      request.type = asset.type; request.gzip = true;
      request.response.assign(reinterpret_cast<const char *>(asset.data), asset.size);
      return true;
    }
    JsonDocument document;
    auto output = document.to<JsonObject>();
    if (request.uri == "/setup/status") {
      output["state"] = phone_setup::state_name(phone_state_);
      output["setup_ssid"] = phone_ssid_;
      output["speaker_url"] = "http://" + hostname_ + "/";
      output["password_set"] = password_record_ == ::openathan::LoadResult::LOADED;
      output["error"] = phone_error_;
    } else if (request.uri == "/setup/networks") {
      output["scanning"] = scanning_ && phone_scan_;
      auto networks = output["networks"].to<JsonArray>();
      for (const auto &network : phone_networks_) {
        auto entry = networks.add<JsonObject>();
        entry["ssid"] = network.ssid; entry["rssi"] = network.rssi; entry["secured"] = network.secured;
      }
    } else { fail(request, 404, "Unknown setup endpoint"); return true; }
    serializeJson(document, request.response);
    return true;
  }
  if (request.method != "POST") { fail(request, 400, "Unsupported setup method"); return true; }
  if (!same_origin(request.origin, request.host, request.site) || request.content_type != "application/json") {
    fail(request, 403, "Same-origin JSON required"); return true;
  }
  if (phone_state_ == phone_setup::State::STORAGE_FAULT || !store_.healthy()) {
    fail(request, 503, "Settings storage failed. Restart or use website USB recovery"); return true;
  }
  if (phone_response_ || wifi_attempt_.active() || scanning_ || upgrade_.usb_busy()) {
    fail(request, 409, "Wait for the current scan or connection attempt to finish"); return true;
  }
  JsonDocument document;
  if (deserializeJson(document, request.body) || !document.is<JsonObject>()) {
    fail(request, 400, "A JSON object is required"); return true;
  }
  const auto input = document.as<JsonObjectConst>();
  const auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  if (request.uri == "/setup/scan" && input.size() == 0) {
    phone_networks_.clear(); phone_scan_ = scanning_ = true;
    scan_deadline_ = now + 10000;
    wifi::global_wifi_component->start_scanning();
    request.code = 202; request.response = "{\"scanning\":true}";
    return true;
  }
  if (request.uri != "/setup/connect" || input.size() != 3 || !input["password"].is<const char *>() ||
      !input["ssid"].is<const char *>() || !input["wifi_password"].is<const char *>()) {
    fail(request, 400, "Provide the device password, Wi-Fi name and Wi-Fi password"); return true;
  }
  if (now - phone_auth_window_ >= 60000) { phone_auth_window_ = now; phone_auth_attempts_ = 0; }
  if (++phone_auth_attempts_ > 10) { fail(request, 429, "Too many password attempts. Wait one minute and try again"); return true; }
  std::string password = input["password"].as<std::string>();
  WifiCredentials candidate{input["ssid"].as<std::string>(), input["wifi_password"].as<std::string>()};
  ClearBody clear_password{password}, clear_wifi_password{candidate.password};
  if (!valid_password(password) || !valid_wifi(candidate.ssid, candidate.password)) {
    std::fill(password.begin(), password.end(), '\0');
    std::fill(candidate.password.begin(), candidate.password.end(), '\0');
    document.clear(); fail(request, 400, "Check the device password and password-protected Wi-Fi details"); return true;
  }
  auto verifier = md5_hex("admin:" + realm_ + ":" + password);
  ClearBody clear_verifier{verifier};
  std::fill(password.begin(), password.end(), '\0'); password.clear(); document.clear();
  if (password_record_ == ::openathan::LoadResult::LOADED && !phone_setup::equal_verifier(verifier, password_.ha1)) {
    std::fill(verifier.begin(), verifier.end(), '\0');
    std::fill(candidate.password.begin(), candidate.password.end(), '\0');
    fail(request, 403, "Enter the existing device password. Use website USB recovery if it is forgotten"); return true;
  }
  if (password_record_ == ::openathan::LoadResult::EMPTY) {
    PasswordVerifier created{1, verifier};
    if (!store_.save_password(created)) {
      std::fill(created.ha1.begin(), created.ha1.end(), '\0');
      std::fill(candidate.password.begin(), candidate.password.end(), '\0');
      password_record_ = ::openathan::LoadResult::ERROR;
      phone_state_ = phone_setup::State::STORAGE_FAULT;
      phone_error_ = "Device password could not be saved. Restart or use website USB recovery.";
      fail(request, 503, phone_error_.c_str()); return true;
    }
    password_ = std::move(created); password_record_ = ::openathan::LoadResult::LOADED;
    auth_.configure(realm_, password_.ha1);
  }
  std::fill(verifier.begin(), verifier.end(), '\0');
  phone_candidate_ = std::move(candidate);
  phone_state_ = phone_setup::State::CONNECTING; phone_error_.clear(); phone_response_at_ = now;
  request.phone_connect = true;
  request.code = 202;
  JsonDocument response;
  response["state"] = "connecting"; response["speaker_url"] = "http://" + hostname_ + "/";
  serializeJson(response, request.response);
  return true;
}
}  // namespace esphome::openathan_device
