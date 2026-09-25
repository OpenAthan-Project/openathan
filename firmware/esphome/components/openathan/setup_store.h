#pragma once
#include <nvs.h>

#include "openathan/settings.h"

namespace esphome::openathan_component {
// Product-only activation gate. Development consumers do not instantiate it.
class SetupStore {
 public:
  ~SetupStore() {
    if (opened_) nvs_close(handle_);
  }
  bool begin(::openathan::LoadResult settings, ::openathan::LoadResult history);
  bool activate();
  bool active() const { return healthy_ && active_; }
  bool healthy() const { return healthy_; }

 private:
  bool write_(bool active);
  nvs_handle_t handle_{};
  bool opened_{}, healthy_{}, active_{};
};
}  // namespace esphome::openathan_component
