#pragma once
#include "esphome/core/defines.h"
#ifndef OPENATHAN_UPGRADE_DESCRIPTOR
#define OPENATHAN_UPGRADE_DESCRIPTOR "upgrade.json"
#endif
#ifndef OPENATHAN_UPGRADE_APPLICATION
#define OPENATHAN_UPGRADE_APPLICATION "firmware.ota.bin"
#endif
#ifndef OPENATHAN_HARDWARE
#define OPENATHAN_HARDWARE "atoms3r-c126-pyramid-a167"
#endif
#ifndef OPENATHAN_UPDATES_ENABLED
#define OPENATHAN_UPDATES_ENABLED 1
#endif
#include "upgrade_api.h"
#include "upgrade_policy.h"
#include <atomic>
#include <mutex>
#include <nvs.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>
#include "openathan/usb_upgrade.h"
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
  ~Upgrade() override;
  void begin(openathan_component::OpenAthan *athan, bool server_ready);
  void loop(bool connected);
  void shutdown() { cancel_ = true; safe_ = false; }
  void snapshot(JsonObject root) override;
  int action(const std::string &action, JsonObjectConst input, std::string &error) override;
  std::vector<std::string> usb_info();
  uint8_t usb_command(uint8_t command, const std::vector<std::string> &fields, std::vector<std::string> &reply);
  bool usb_chunk(const ::openathan::usb_upgrade::Chunk &chunk);
  bool usb_busy() const { return usb_token_ || usb_request_; }
 private:
  static void worker_(void *argument);
  void run_();
  bool start_(bool install);
  bool descriptor_(const std::string &envelope, UpgradeRelease &release);
  bool persist_(const std::string &queue, const std::string &expected);
  void fail_(const char *message);
  enum class UsbSelection { UNKNOWN, SELECTED, UNSELECTED, REJECTED };
  UsbSelection usb_selection_();
  bool usb_safe_();
  bool usb_abort_();
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
  bool usb_request_{};
  uint64_t usb_token_{}, usb_last_ms_{};
  uint32_t usb_descriptor_bytes_{};
  std::string usb_descriptor_;
  uint32_t usb_handle_{};
  mbedtls_sha256_context usb_hash_{};
  bool usb_hash_live_{};
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
  friend class openathan_upgrade_qualification::Qualification;
  openathan_upgrade_qualification::Qualification qualification_;
#endif
};
}  // namespace esphome::openathan_device
