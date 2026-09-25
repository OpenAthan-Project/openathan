#pragma once
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <deque>
#include <memory>
#include <mutex>

#include "credential_store.h"
#include "digest.h"
#include "esphome/components/openathan/openathan.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "local_api.h"
#include "protocol.h"
#include "wifi_attempt.h"

namespace esphome::openathan_device {
struct Asset {
  const uint8_t* data{};
  size_t size{};
  const char* type{};
};
struct HttpExchange : public ApiExchange {
  HttpExchange() : done(xSemaphoreCreateBinary()) {}
  ~HttpExchange() {
    if (done) vSemaphoreDelete(done);
  }
  SemaphoreHandle_t done;
  std::string host, origin, site, authorization, content_type;
  std::string type{"application/json"}, challenge;
  bool gzip{};
  // This lock serializes timeout/cancellation with a mutating main-loop action.
  std::mutex mutex;
  bool cancelled{};
};
class Device : public Component, public wifi::WiFiScanResultsListener, public wifi::WiFiConnectStateListener {
 public:
  void set_openathan(openathan_component::OpenAthan* value) {
    athan_ = value;
    athan_->require_setup();
  }
  void set_zones(const ZoneEntry* value, size_t count) {
    zones_ = value;
    zone_count_ = count;
  }
  void set_asset(unsigned index, const uint8_t* value, size_t count, const char* type) {
    assets_[index] = {value, count, type};
  }
  void set_sender(std::function<void(const std::vector<uint8_t>&)> sender) { sender_ = std::move(sender); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void serial_request(const Frame& request);
  void on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult>& results) override;
  void on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) override;

 private:
  static esp_err_t http_handler_(httpd_req_t* request);
  void handle_http_(HttpExchange& request);
  bool valid_host_(const std::string& host) const;
  void send_(bool extension, uint8_t type, const std::vector<uint8_t>& data);
  void result_(bool extension, uint8_t command, const std::vector<std::string>& fields);
  void restore_wifi_();
  std::string ip_() const;
  std::vector<std::string> urls_() const;
  uint8_t wifi_state_() const;
  openathan_component::OpenAthan* athan_{};
  CredentialStore store_;
  WifiCredentials saved_wifi_;
  WifiAttempt wifi_attempt_;
  PasswordVerifier password_;
  ::openathan::LoadResult wifi_record_{::openathan::LoadResult::EMPTY},
      password_record_{::openathan::LoadResult::EMPTY};
  DigestAuth auth_;
  std::string realm_, hostname_;
  std::function<void(const std::vector<uint8_t>&)> sender_;
  bool scanning_{};
  bool maintenance_{};
  uint64_t scan_deadline_{}, auth_window_{};
  unsigned auth_attempts_{};
  const ZoneEntry* zones_{};
  size_t zone_count_{};
  Asset assets_[3];
  std::unique_ptr<LocalApi> api_;
  httpd_handle_t server_{};
  std::mutex queue_mutex_;
  std::deque<std::shared_ptr<HttpExchange>> requests_;
};
}  // namespace esphome::openathan_device
