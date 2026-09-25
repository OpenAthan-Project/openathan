#pragma once
#include "openathan/scheduler_state.h"
#include "storage_config.h"
#include <nvs.h>
#include <string>

namespace esphome::openathan_component {
class NvsStateStore : public ::openathan::StateStore {
 public:
  explicit NvsStateStore(std::string storage_namespace = openathan_storage::PRAYER) : namespace_(std::move(storage_namespace)) {}
  NvsStateStore(const NvsStateStore &) = delete;
  NvsStateStore &operator=(const NvsStateStore &) = delete;
  ~NvsStateStore() override { if (opened_) nvs_close(handle_); }
  ::openathan::LoadResult load(::openathan::DurableState &state) override;
  bool save(const ::openathan::DurableState &state) override;
 private:
  std::string namespace_;
  nvs_handle_t handle_{};
  bool opened_{false};
};
}  // namespace esphome::openathan_component
