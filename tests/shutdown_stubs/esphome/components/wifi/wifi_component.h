#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace esphome {
using StringRef = std::string_view;
namespace wifi {
struct WiFiScanResult {};
template<typename T> using wifi_scan_vector_t = std::vector<T>;
class WiFiScanResultsListener {
 public:
  virtual ~WiFiScanResultsListener() = default;
  virtual void on_wifi_scan_results(const wifi_scan_vector_t<WiFiScanResult>&) = 0;
};
class WiFiConnectStateListener {
 public:
  virtual ~WiFiConnectStateListener() = default;
  virtual void on_wifi_connect_state(StringRef, std::span<const uint8_t,6>) = 0;
};
}
}
