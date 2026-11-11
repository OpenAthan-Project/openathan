#pragma once

#ifdef USE_OPENATHAN_SPEAKER_DISCOVERY

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "esphome/core/component.h"

namespace esphome::openathan_device {
class SpeakerDiscovery : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void on_shutdown() override;
  void update(std::string_view ip, std::string_view name);
  std::string directory(std::string_view ip);

 private:
  void run_();

  std::mutex mutex_;
  std::string hostname_, ip_, name_, directory_;
  std::optional<TickType_t> directory_withdrawn_at_;
  TaskHandle_t worker_{};
  std::atomic<bool> stopping_{};
  unsigned network_generation_{};
};
}  // namespace esphome::openathan_device

#endif
