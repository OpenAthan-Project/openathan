#pragma once
#include "dua_api.h"
#include "esphome/components/openathan/openathan.h"
#include "openathan/dua_media.h"
#include <atomic>
#include <mutex>
#include <esp_partition.h>
#include <nvs.h>
namespace esphome::openathan_device {
class DuaMedia : public DuaApi {
 public:
  void begin(openathan_component::OpenAthan *athan);
  void loop(bool connected, bool permitted);
  void shutdown() { safe_ = false; cancel_ = true; }
  void preference_changed() override { safe_ = false; cancel_ = true; }
  bool active() const { return active_; }
  void snapshot(JsonObject root) override;
  int retry(std::string &error) override;
 private:
  static void worker_(void *argument);
  void run_();
  void fail_(const char *error, bool transient);
  bool verify_(const char *envelope, size_t size, uint32_t bytes, const uint8_t *digest,
               bool exact_release = false);
  bool scan_();
  bool allowed_() const { return safe_ && !cancel_; }
  openathan_component::OpenAthan *athan_{};
  const esp_partition_t *partition_{};
  const uint8_t *data_{};
  esp_partition_mmap_handle_t mapping_{};
  std::optional<::openathan::DuaBank> installed_;
  nvs_handle_t journal_{};
  bool storage_ok_{}, compatible_{}, latched_{}, success_pending_{};
  unsigned target_{}, staging_{};
  uint64_t retry_at_{};
  std::atomic<bool> active_{}, cancel_{}, safe_{};
  std::mutex mutex_;
  std::string state_{"unavailable"}, error_;
  uint32_t received_{}, total_{};
};
}
