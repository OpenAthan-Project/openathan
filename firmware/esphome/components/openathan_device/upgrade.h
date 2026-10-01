#pragma once
#include "esphome/core/defines.h"
#include "upgrade_api.h"
#include "upgrade_policy.h"
#include <atomic>
#include <mutex>
#include <nvs.h>
#include <esp_partition.h>
#include "esphome/components/openathan/openathan.h"
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
#include "esphome/components/openathan_upgrade_qualification/qualification.h"
#endif

namespace esphome::openathan_device {
struct UpgradeRelease {
  std::string version, commit, sha256, envelope;
  uint32_t bytes{};
};
class Upgrade : public UpgradeApi {
 public:
  void begin(openathan_component::OpenAthan *athan, bool server_ready);
  void loop(bool connected);
  void shutdown() { cancel_ = true; safe_ = false; }
  void snapshot(JsonObject root) override;
  int action(const std::string &action, JsonObjectConst input, std::string &error) override;
 private:
  static void worker_(void *argument);
  void run_();
  bool start_(bool install);
  bool descriptor_(const std::string &envelope, UpgradeRelease &release);
  bool persist_(const std::string &queue, const std::string &expected);
  void fail_(const char *message);
  openathan_component::OpenAthan *athan_{};
  std::mutex mutex_;
  UpgradeRelease offered_, queued_;
  std::string state_{"idle"}, error_, expected_, result_;
  const esp_partition_t *staged_{};
  nvs_handle_t nvs_{};
  bool storage_ok_{}, server_ready_{}, confirmed_{}, install_job_{}, bootloader_ok_{};
  std::atomic<bool> active_{false}, cancel_{false}, safe_{false};
  std::atomic<uint32_t> received_{0};
  uint64_t boot_ms_{}, next_check_ms_{}, retry_ms_{};
  int64_t last_check_{};
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
  friend class openathan_upgrade_qualification::Qualification;
  openathan_upgrade_qualification::Qualification qualification_;
#endif
};
}  // namespace esphome::openathan_device
