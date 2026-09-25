#include "nvs_state_store.h"

namespace esphome::openathan_component {
::openathan::LoadResult NvsStateStore::load(::openathan::DurableState &state) {
  using ::openathan::LoadResult;
  state = {};
  // ESPHome initializes NVS. Never erase or reset storage on a read failure.
  if (!opened_) {
    if (nvs_open(namespace_.c_str(), NVS_READWRITE, &handle_) != ESP_OK) return LoadResult::ERROR;
    opened_ = true;
  }
  size_t length = 0;
  auto err = nvs_get_blob(handle_, "scheduler", nullptr, &length);
  if (err == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  ::openathan::StateRecord record;
  if (err != ESP_OK || length != record.size()) return LoadResult::ERROR;
  err = nvs_get_blob(handle_, "scheduler", record.data(), &length);
  if (err != ESP_OK || length != record.size() || !::openathan::decode_state(record, state)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool NvsStateStore::save(const ::openathan::DurableState &state) {
  if (!opened_) return false;
  const auto record = ::openathan::encode_state(state);
  if (nvs_set_blob(handle_, "scheduler", record.data(), record.size()) != ESP_OK) return false;
  return nvs_commit(handle_) == ESP_OK;
}
}  // namespace esphome::openathan_component
