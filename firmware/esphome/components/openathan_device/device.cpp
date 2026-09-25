#include "device.h"

#include <esp_random.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "esphome/components/wifi/scan_list.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "test_maintenance.h"

namespace esphome::openathan_device {
namespace {
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
std::string random_nonce() {
  uint8_t bytes[24];
  esp_fill_random(bytes, sizeof(bytes));
  const char* hex = "0123456789abcdef";
  std::string result;
  for (auto byte : bytes) {
    result += hex[byte >> 4];
    result += hex[byte & 15];
  }
  return result;
}
std::string header(httpd_req_t* request, const char* name, size_t limit = 2048) {
  const auto size = httpd_req_get_hdr_value_len(request, name);
  if (!size || size > limit) return {};
  std::string result(size + 1, '\0');
  if (httpd_req_get_hdr_value_str(request, name, result.data(), result.size()) != ESP_OK) return {};
  result.resize(size);
  return result;
}
void error(HttpExchange& request, int code, const char* message) {
  request.code = code;
  JsonDocument doc;
  doc["error"] = message;
  request.response.clear();
  serializeJson(doc, request.response);
}
const char* http_status(int code) {
  switch (code) {
    case 200:
      return "200 OK";
    case 400:
      return "400 Bad Request";
    case 401:
      return "401 Unauthorized";
    case 403:
      return "403 Forbidden";
    case 404:
      return "404 Not Found";
    case 409:
      return "409 Conflict";
    case 413:
      return "413 Content Too Large";
    case 429:
      return "429 Too Many Requests";
    default:
      return "503 Service Unavailable";
  }
}

}  // namespace
void Device::setup() {
  api_ = std::make_unique<LocalApi>(athan_, zones_, zone_count_);
  hostname_ = std::string(App.get_name().c_str()) + ".local";
  realm_ = "OpenAthan-" + get_mac_address();
  wifi_record_ = store_.load_wifi(saved_wifi_);
  password_record_ = store_.load_password(password_);
  auth_.configure(realm_, password_record_ == ::openathan::LoadResult::LOADED ? password_.ha1 : "");
  auto* wifi = wifi::global_wifi_component;
  wifi->add_scan_results_listener(this);
  wifi->add_connect_state_listener(this);
  restore_wifi_();
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;
  config.max_open_sockets = 4;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 3;
  config.send_wait_timeout = 3;
  config.uri_match_fn = httpd_uri_match_wildcard;
  if (httpd_start(&server_, &config) != ESP_OK) {
    mark_failed();
    return;
  }
  httpd_uri_t handler{};
  handler.uri = "/*";
  handler.method = static_cast<httpd_method_t>(HTTP_ANY);
  handler.handler = http_handler_;
  handler.user_ctx = this;
  if (httpd_register_uri_handler(server_, &handler) != ESP_OK) {
    httpd_stop(server_);
    server_ = nullptr;
    mark_failed();
  }
}
void Device::on_shutdown() {
  if (server_) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}
void Device::restore_wifi_() {
  auto* wifi = wifi::global_wifi_component;
  wifi->disable();
  wifi->clear_sta();
  if (wifi_record_ == ::openathan::LoadResult::LOADED) {
    wifi::WiFiAP network;
    network.set_ssid(saved_wifi_.ssid);
    network.set_password(saved_wifi_.password);
    network.set_hidden(true);
    wifi->set_sta(network);
  }
  wifi->enable();
  if (wifi_record_ != ::openathan::LoadResult::LOADED) wifi->start_scanning();
}
std::string Device::ip_() const {
  char address[network::IP_ADDRESS_BUFFER_SIZE];
  wifi::global_wifi_component->get_ip_addresses()[0].str_to(address);
  return address;
}
std::vector<std::string> Device::urls_() const {
  if (!wifi::global_wifi_component->is_connected()) return {};
  return {"http://" + hostname_ + "/", "http://" + ip_() + "/"};
}
uint8_t Device::wifi_state_() const {
  return wifi_attempt_.active()
             ? 3
             : (wifi_record_ == ::openathan::LoadResult::LOADED && wifi::global_wifi_component->is_connected() ? 4 : 2);
}
void Device::send_(bool extension, uint8_t type, const std::vector<uint8_t>& data) {
  if (sender_) sender_(frame(extension, type, data));
}
void Device::result_(bool extension, uint8_t command, const std::vector<std::string>& fields) {
  send_(extension, 4, rpc(command, fields));
}
void Device::serial_request(const Frame& request) {
  if (request.type != 3) return;
  uint8_t command;
  std::vector<std::string> fields;
  if (!parse_rpc(request.data, command, fields)) {
    send_(request.extension, 2, {1});
    return;
  }
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
  if (request.extension && command == 0x70 && fields.empty()) {
    result_(true, command, {"test-v1", hostname_, maintenance_ ? "restart_required" : "ready"});
    return;
  }
  if (request.extension && command == 0x71) {
    if (maintenance_) {
      send_(true, 2, {255});
      return;
    }
    const auto result = clear_test_storage(fields, hostname_, [this]() {
      maintenance_ = true;
      athan_->quiesce_for_maintenance();
      wifi_attempt_.finish();
      scanning_ = false;
    });
    if (result == ClearResult::CLEARED)
      result_(true, command, {"cleared", "restart_required"});
    else
      send_(true, 2, {uint8_t(result == ClearResult::INVALID ? 1 : 255)});
    return;
  }
#endif
  if (maintenance_) {
    send_(request.extension, 2, {255});
    return;
  }
  send_(request.extension, 2, {0});
  if (request.extension) {
    if (command == 1 && fields.empty()) {
      std::vector<std::string> status{"1",
                                      std::to_string(wifi_state_()),
                                      password_record_ == ::openathan::LoadResult::LOADED  ? "ready"
                                      : password_record_ == ::openathan::LoadResult::EMPTY ? "absent"
                                                                                           : "fault",
                                      athan_->setup_state(),
                                      std::to_string(password_.revision),
                                      hostname_,
                                      !store_.healthy() || wifi_record_ == ::openathan::LoadResult::ERROR ||
                                              password_record_ == ::openathan::LoadResult::ERROR
                                          ? "fault"
                                          : "ready"};
      const auto urls = urls_();
      status.insert(status.end(), urls.begin(), urls.end());
      result_(true, command, status);
    } else if (command == 2 && fields.size() == 1 && valid_password(fields[0])) {
      if (password_.revision == std::numeric_limits<uint32_t>::max()) {
        send_(true, 2, {255});
        return;
      }
      PasswordVerifier candidate{password_.revision + 1, md5_hex("admin:" + realm_ + ":" + fields[0])};
      std::fill(fields[0].begin(), fields[0].end(), 0);
      if (!store_.save_password(candidate)) {
        password_record_ = ::openathan::LoadResult::ERROR;
        auth_.configure(realm_, "");
        send_(true, 2, {255});
        return;
      }
      password_ = candidate;
      password_record_ = ::openathan::LoadResult::LOADED;
      auth_.configure(realm_, password_.ha1);
      result_(true, command, {"saved", std::to_string(password_.revision)});
    } else
      send_(true, 2, {uint8_t(command == 2 ? 1 : 2)});
    return;
  }
  if (command == 1) {
    if (fields.size() != 2 || !valid_wifi(fields[0], fields[1])) {
      send_(false, 2, {1});
      return;
    }
    if (wifi_attempt_.active() || scanning_) {
      send_(false, 2, {255});
      return;
    }
    if (!wifi_attempt_.begin({fields[0], fields[1]}, now_ms())) {
      send_(false, 2, {1});
      return;
    }
    wifi::global_wifi_component->disable();
    send_(false, 1, {3});
  } else if (!fields.empty())
    send_(false, 2, {1});
  else if (command == 2) {
    send_(false, 1, {wifi_state_()});
    if (wifi_state_() == 4) result_(false, command, urls_());
  } else if (command == 3)
    result_(false, command, {"OpenAthan", "1-dev", "ESP32-S3", hostname_});
  else if (command == 4) {
    if (wifi_attempt_.active() || scanning_) {
      send_(false, 2, {255});
      return;
    }
    scanning_ = true;
    scan_deadline_ = now_ms() + 10000;
    wifi::global_wifi_component->start_scanning();
  } else if (command == 7) {
    auto fields = urls_();
    fields.insert(fields.begin(), wifi::global_wifi_component->is_connected() ? "3" : "2");
    result_(false, command, fields);
  } else
    send_(false, 2, {2});
}
void Device::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6>) {
  wifi_attempt_.joined(std::string(ssid.c_str()));
}
void Device::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult>& results) {
  if (!scanning_) return;
  unsigned count = 0;
  for (const auto& scan : results) {
    bool secured = false;
    if (!wifi::should_show_scan_entry(results, scan, secured)) continue;
    result_(false, 4, {scan.get_ssid().c_str(), std::to_string(scan.get_rssi()), secured ? "YES" : "NO"});
    if (++count >= 32) break;
  }
  result_(false, 4, {});
  scanning_ = false;
}
void Device::loop() {
  const auto now = now_ms();
  if (wifi_attempt_.active()) {
    auto* wifi = wifi::global_wifi_component;
    char ssid[wifi::SSID_BUFFER_SIZE];
    const auto result = wifi_attempt_.step(now, wifi->is_connected(), wifi->wifi_ssid_to(ssid));
    if (result == WifiAttempt::Result::START_CONNECTION) {
      wifi->clear_sta();
      wifi::WiFiAP network;
      network.set_ssid(wifi_attempt_.candidate().ssid);
      network.set_password(wifi_attempt_.candidate().password);
      network.set_hidden(true);
      wifi->set_sta(network);
      wifi->enable();
    } else if (result == WifiAttempt::Result::CONNECTED) {
      const bool saved = store_.save_wifi(wifi_attempt_.candidate());
      if (saved) {
        saved_wifi_ = wifi_attempt_.candidate();
        wifi_record_ = ::openathan::LoadResult::LOADED;
      }
      wifi_attempt_.finish();
      if (saved) {
        send_(false, 1, {4});
        result_(false, 1, urls_());
      } else {
        restore_wifi_();
        send_(false, 2, {255});
        send_(false, 1, {wifi_state_()});
      }
    } else if (result == WifiAttempt::Result::TIMED_OUT) {
      wifi_attempt_.finish();
      restore_wifi_();
      send_(false, 2, {3});
      send_(false, 1, {wifi_state_()});
    }
  }
  if (scanning_ && now >= scan_deadline_) {
    scanning_ = false;
    send_(false, 2, {255});
    result_(false, 4, {});
  }
  std::shared_ptr<HttpExchange> request;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!requests_.empty()) {
      request = requests_.front();
      requests_.pop_front();
    }
  }
  if (request) {
    std::lock_guard<std::mutex> lock(request->mutex);
    if (!request->cancelled) handle_http_(*request);
    xSemaphoreGive(request->done);
  }
}
esp_err_t Device::http_handler_(httpd_req_t* raw) {
  auto* self = static_cast<Device*>(raw->user_ctx);
  if (raw->content_len > 4096) {
    httpd_resp_set_status(raw, "413 Content Too Large");
    return httpd_resp_send(raw, "Request too large", HTTPD_RESP_USE_STRLEN);
  }
  auto request = std::make_shared<HttpExchange>();
  if (!request->done) return ESP_FAIL;
  request->uri = raw->uri;
  request->method = raw->method == HTTP_GET ? "GET" : raw->method == HTTP_POST ? "POST" : "OTHER";
  request->host = header(raw, "Host", 128);
  request->origin = header(raw, "Origin", 256);
  request->site = header(raw, "Sec-Fetch-Site", 32);
  request->authorization = header(raw, "Authorization");
  request->content_type = header(raw, "Content-Type", 80);
  request->body.resize(raw->content_len);
  size_t received = 0;
  while (received < request->body.size()) {
    const int size = httpd_req_recv(raw, request->body.data() + received, request->body.size() - received);
    if (size <= 0) return ESP_FAIL;
    received += size;
  }
  {
    std::lock_guard<std::mutex> lock(self->queue_mutex_);
    if (self->requests_.size() >= 4) {
      httpd_resp_set_status(raw, "503 Service Unavailable");
      return httpd_resp_send(raw, "Busy", HTTPD_RESP_USE_STRLEN);
    }
    self->requests_.push_back(request);
  }
  if (xSemaphoreTake(request->done, pdMS_TO_TICKS(5000)) != pdTRUE) {
    std::lock_guard<std::mutex> lock(request->mutex);
    request->cancelled = true;
    httpd_resp_set_status(raw, "503 Service Unavailable");
    return httpd_resp_send(raw, "Read device state before retrying", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_status(raw, http_status(request->code));
  httpd_resp_set_type(raw, request->type.c_str());
  httpd_resp_set_hdr(raw, "Cache-Control", "no-store");
  httpd_resp_set_hdr(raw, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(raw, "Referrer-Policy", "no-referrer");
  httpd_resp_set_hdr(raw, "Content-Security-Policy",
                     "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; "
                     "form-action 'self'; frame-ancestors 'none'");
  if (!request->challenge.empty()) httpd_resp_set_hdr(raw, "WWW-Authenticate", request->challenge.c_str());
  if (request->gzip) httpd_resp_set_hdr(raw, "Content-Encoding", "gzip");
  return httpd_resp_send(raw, request->response.data(), request->response.size());
}
bool Device::valid_host_(const std::string& host) const {
  return host == hostname_ || host == hostname_ + ":80" || host == ip_() || host == ip_() + ":80";
}
void Device::handle_http_(HttpExchange& request) {
  if (maintenance_) {
    error(request, 503, "Test maintenance requires a physical restart");
    return;
  }
  if (!valid_host_(request.host)) {
    error(request, 403, "Use the device's local address");
    return;
  }
  if (request.method != "GET" && request.method != "POST") {
    error(request, 400, "Unsupported method");
    return;
  }
  if (request.method == "POST" &&
      (!same_origin(request.origin, request.host, request.site) || request.content_type != "application/json")) {
    error(request, 403, "Same-origin JSON required");
    return;
  }
  if (!request.origin.empty() && request.origin != "http://" + request.host) {
    error(request, 403, "Cross-origin request rejected");
    return;
  }
  if (!auth_.available()) {
    error(request, 503, "Connect USB and set the device password to unlock local settings");
    return;
  }
  const auto now = now_ms();
  if (now - auth_window_ >= 1000) {
    auth_window_ = now;
    auth_attempts_ = 0;
  }
  if (!request.authorization.empty() && ++auth_attempts_ > 20) {
    error(request, 429, "Too many authentication attempts");
    return;
  }
  if (!auth_.authorize(request.authorization, request.method, request.uri, now)) {
    error(request, 401, "Sign in as admin with your device password");
    request.challenge = auth_.challenge(now, random_nonce());
    return;
  }
  constexpr const char* paths[] = {"/", "/app.js", "/style.css"};
  for (unsigned i = 0; i < 3; ++i)
    if (request.method == "GET" && request.uri == paths[i]) {
      request.type = assets_[i].type;
      request.gzip = true;
      request.response.assign(reinterpret_cast<const char*>(assets_[i].data), assets_[i].size);
      return;
    }
  api_->set_context(hostname_, wifi::global_wifi_component->is_connected());
  api_->handle(request);
}
}  // namespace esphome::openathan_device
