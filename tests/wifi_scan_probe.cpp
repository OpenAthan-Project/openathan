// Compile the installed ESPHome scan methods with a captured ESP-IDF driver call.
// This probes scan selection, not radio operation or delivery over serial.
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#define USE_IMPROV_SERIAL
#define ESP_LOGV(...)
#define ESP_OK 0
#define WIFI_SCAN_TYPE_PASSIVE 1
#define WIFI_SCAN_TYPE_ACTIVE 2
using esp_err_t = int;
struct wifi_scan_config_t {
  uint8_t *ssid{}, *bssid{};
  int channel{}, scan_type{};
  bool show_hidden{};
  struct { int passive{}; struct { int min{}, max{}; } active; } scan_time;
};
static std::string requested_ssid;
static bool restricted;
esp_err_t esp_wifi_scan_start(wifi_scan_config_t *config, bool) {
  restricted = config->ssid != nullptr;
  requested_ssid = restricted ? reinterpret_cast<char *>(config->ssid) : "";
  return ESP_OK;
}
namespace improv_serial {
int component;
void *global_improv_serial_component = &component;
}
class WiFiComponent {
 public:
  struct AP {
    std::string ssid{"saved-network"};
    const std::string &get_ssid() const { return ssid; }
  };
  bool keep_scan_results_{}, connected_{true}, scan_driver_filtered_{}, scan_done_{};
  std::vector<AP> sta_{AP{}};
  bool is_connected() const { return connected_; }
  bool wifi_mode_(bool, bool = false) { return true; }
  bool needs_full_scan_results_() const;
  bool wifi_scan_start_(bool passive);
};
#include "wifi_scan_functions.inc"

int main() {
  WiFiComponent wifi;
  // Reproduce the original connected recovery bug before applying the flag.
  assert(wifi.wifi_scan_start_(false));
  assert(restricted && requested_ssid == "saved-network" && wifi.scan_driver_filtered_);
  wifi.keep_scan_results_ = true;
  assert(wifi.wifi_scan_start_(false));
  assert(!restricted && !wifi.scan_driver_filtered_);
  // Initial disconnected Improv discovery remains unrestricted, with either flag.
  wifi.connected_ = false;
  assert(wifi.wifi_scan_start_(false));
  assert(!restricted && !wifi.scan_driver_filtered_);
  wifi.keep_scan_results_ = false;
  assert(wifi.wifi_scan_start_(false));
  assert(!restricted && !wifi.scan_driver_filtered_);
}
