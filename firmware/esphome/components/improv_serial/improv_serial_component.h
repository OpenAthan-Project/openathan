#pragma once
#include <deque>

#include "esphome/components/openathan_device/device.h"
#include "esphome/core/component.h"

namespace esphome::improv_serial {
class ImprovSerialComponent : public Component {
 public:
  void set_device(openathan_device::Device* device) { device_ = device; }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI + 1; }
  void setup() override;
  void loop() override;

 private:
  openathan_device::Device* device_{};
  openathan_device::SerialParser parser_;
  std::deque<std::vector<uint8_t>> outgoing_;
  size_t sent_{};
  uint64_t progress_{};
};
extern ImprovSerialComponent* global_improv_serial_component;
}  // namespace esphome::improv_serial
