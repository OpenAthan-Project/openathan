#include "improv_serial_component.h"

#include <driver/usb_serial_jtag.h>
#include <esp_timer.h>

namespace esphome::improv_serial {
ImprovSerialComponent* global_improv_serial_component = nullptr;
void ImprovSerialComponent::setup() {
  global_improv_serial_component = this;
  device_->set_sender([this](const std::vector<uint8_t>& bytes) {
    if (outgoing_.size() >= 64) return;
    if (outgoing_.empty()) progress_ = esp_timer_get_time() / 1000;
    outgoing_.push_back(bytes);
  });
}
void ImprovSerialComponent::loop() {
  // Bound work per iteration so serial traffic cannot starve the scheduler.
  uint8_t bytes[128];
  const int count = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), 0);
  for (int i = 0; i < count; ++i) {
    auto request = parser_.feed(bytes[i], esp_timer_get_time() / 1000);
    if (request) device_->serial_request(*request);
  }
  const uint64_t now = esp_timer_get_time() / 1000;
  if (!outgoing_.empty()) {
    const auto& bytes = outgoing_.front();
    const int written = usb_serial_jtag_write_bytes(bytes.data() + sent_, bytes.size() - sent_, 0);
    if (written > 0) {
      sent_ += written;
      progress_ = now;
      if (sent_ == bytes.size()) {
        outgoing_.pop_front();
        sent_ = 0;
      }
    } else if (now - progress_ > 1000) {
      outgoing_.clear();
      sent_ = 0;
    }
  }
}
}  // namespace esphome::improv_serial
