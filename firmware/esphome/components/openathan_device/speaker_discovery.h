#pragma once

#ifdef USE_OPENATHAN_SPEAKER_DISCOVERY

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mutex>
#include <string>
#include <string_view>

namespace esphome::openathan_device {
class SpeakerDiscovery {
 public:
  void update(std::string_view hostname, std::string_view ip, std::string_view name);
  std::string directory(std::string_view ip);

 private:
  void run_();

  std::mutex mutex_;
  std::string hostname_, ip_, name_, directory_;
  TaskHandle_t worker_{};
  unsigned network_generation_{};
};
}  // namespace esphome::openathan_device

#endif
