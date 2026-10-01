// Compile the actual device lifecycle against a pending HTTP-request adapter.
// HTTP stop models the pinned server's wait for the main-loop request timeout.
#include "device.h"
#include <cassert>
#include <iostream>
namespace esphome::openathan_device {
void Device::setup() {
  server_ = this;  // Started server, not the uninitialized nullptr case.
  std::string error; upgrade_.action("arm_safe_worker", {}, error);
}
void Device::loop() {
  std::string error; assert(upgrade_.action("inspect_shutdown", {}, error) == 200);
}
void Device::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult>&) {}
void Device::on_wifi_connect_state(StringRef, std::span<const uint8_t,6>) {}
void Upgrade::snapshot(JsonObject) {}
int Upgrade::action(const std::string& action, JsonObjectConst, std::string&) {
  if (action == "arm_safe_worker") { cancel_ = false; safe_ = true; return 200; }
  return cancel_.load() && !safe_.load() ? 200 : 503;
}
}
void nvs_close(nvs_handle_t) {}
int main() {
  using esphome::openathan_device::Device;
  Device device; device.setup(); pending_main_loop_request = true;
  device.on_shutdown(); // Must return with no wait for the now-stopped loop.
  device.loop(); // Real Upgrade::shutdown must cancel and revoke the safe window.
  assert(stop_calls == 0);
  Device idle; idle.on_shutdown(); idle.loop();
  std::cout << "Device shutdown returns with a pending authenticated HTTP request and cancels updater work\n";
}
