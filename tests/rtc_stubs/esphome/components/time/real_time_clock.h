#pragma once
#include "esphome/core/component.h"
#include <esp_sntp.h>
#include <ctime>
#include <functional>
#include <utility>
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
  virtual void dump_config() {}
  ESPTime now() { return utcnow(); }
  ESPTime utcnow() { return ESPTime::from_epoch_utc(system_epoch); }
  struct Callbacks {
    std::vector<std::function<void()>> values;
    void call() { for (auto &callback : values) callback(); }
  } time_sync_callback_;
  template<typename F> void add_on_time_sync_callback(F callback) { time_sync_callback_.values.emplace_back(callback); }
  void network_sync(time_t epoch) {
    system_epoch = epoch;
    sntp_test::status = SNTP_SYNC_STATUS_COMPLETED;
    time_sync_callback_.call();
  }
  void disable_loop() { loop_disabled = true; }
  unsigned get_update_interval() const { return 900000; }
  template<typename F> void defer(F callback) { deferred.emplace_back(callback); }
  void run_deferred() { auto pending = std::move(deferred); for (auto &callback : pending) callback(); }
  bool loop_disabled{};
  std::vector<std::function<void()>> deferred;
 protected:
  void synchronize_epoch_(uint32_t epoch) { system_epoch = epoch; time_sync_callback_.call(); }
};
}
}
