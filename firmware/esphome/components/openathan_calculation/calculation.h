#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "openathan/prayer_calculator.h"

namespace esphome::openathan_calculation {
class Calculation : public Component {
 public:
  void setup() override {
    // A fixed upstream reference case, NOT the user's configured location/date.
    ::openathan::PrayerDay times;
    if (!::openathan::calculate_prayers({35.775, -78.6336, 2015, 7, 12,
            ::openathan::Method::NORTH_AMERICA, true}, times)) {
      ESP_LOGE("openathan_calc", "Reference calculation failed");
      this->mark_failed();
      return;
    }
    for (size_t i = 0; i < times.size(); ++i) {
      if (times[i])
        ESP_LOGI("openathan_calc", "Reference event %u: UTC Unix seconds %lld", unsigned(i),
                 static_cast<long long>(*times[i]));
      else
        ESP_LOGW("openathan_calc", "Reference event %u unavailable", unsigned(i));
    }
  }
};
}  // namespace esphome::openathan_calculation
