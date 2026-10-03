#pragma once
namespace esphome::wifi {
class WiFiComponent {
 public:
  bool connected{true};
  bool is_connected() const { return connected; }
};
inline WiFiComponent *global_wifi_component{};
}
