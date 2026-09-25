#pragma once
#include "../openathan/openathan.h"

namespace esphome::openathan_device {
struct ZoneEntry {
  const char* name;
  const char* rules;
};
struct ApiExchange {
  std::string method, uri, body, response;
  int code{200};
};
// Called only on the ESPHome main loop, after HTTP authentication/origin checks.
// Keeping this adapter independent of the HTTP/Wi-Fi tasks lets tests execute
// the production JSON endpoints against deterministic clocks and NVS failures.
class LocalApi {
 public:
  LocalApi(openathan_component::OpenAthan* athan, const ZoneEntry* zones, size_t count)
      : athan_(athan), zones_(zones), zone_count_(count) {}
  void set_context(std::string hostname, bool connected) {
    hostname_ = std::move(hostname);
    wifi_connected_ = connected;
  }
  void handle(ApiExchange& request);

 private:
  void snapshot_(JsonObject root);
  void preview_(JsonObject root, const ::openathan::DeviceSettings& settings);
  bool resolve_timezone_(JsonObject settings);
  openathan_component::OpenAthan* athan_;
  const ZoneEntry* zones_;
  size_t zone_count_;
  std::string hostname_;
  bool wifi_connected_{};
};
}  // namespace esphome::openathan_device
