#pragma once
#include "esphome/core/component.h"
#include <ctime>
#include <functional>
#include <vector>

namespace esphome {
struct ESPTime {
  unsigned second{}, minute{}, hour{}, day_of_week{}, day_of_month{}, month{}, year{};
  time_t timestamp{};
  bool is_valid() const { return year >= 2019; }
  static ESPTime from_epoch_utc(time_t epoch) {
    const auto value = *std::gmtime(&epoch);
    return {unsigned(value.tm_sec), unsigned(value.tm_min), unsigned(value.tm_hour), unsigned(value.tm_wday + 1),
        unsigned(value.tm_mday), unsigned(value.tm_mon + 1), unsigned(value.tm_year + 1900), epoch};
  }
};
namespace time {
inline time_t system_epoch{};
class RealTimeClock : public Component {
 public:
  virtual void update() {}
  ESPTime utcnow() { return ESPTime::from_epoch_utc(system_epoch); }
  std::vector<std::function<void()>> callbacks;
  template<typename F> void add_on_time_sync_callback(F callback) { callbacks.emplace_back(callback); }
  void network_sync(time_t epoch) { system_epoch = epoch; for (auto &callback : callbacks) callback(); }
 protected:
  void synchronize_epoch_(uint32_t epoch) { system_epoch = epoch; for (auto &callback : callbacks) callback(); }
};
}
}
