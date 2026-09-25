#pragma once
#include <cstdint>
#include <functional>
namespace esphome {
struct ESPTime {
  int64_t epoch;
  static ESPTime from_epoch_utc(int64_t epoch) { return {epoch}; }
  bool is_valid() const { return epoch >= 1546300800; }
};
namespace time {
class RealTimeClock {
 public:
  void add_on_time_sync_callback(std::function<void()> callback) { callback_ = callback; }
  std::function<void()> callback_;
};
}
}
