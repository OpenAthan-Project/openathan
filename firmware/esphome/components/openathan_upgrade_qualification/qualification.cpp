#include "qualification.h"
#include "esphome/components/openathan_device/upgrade.h"
#include "esphome/core/application.h"
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <array>

namespace esphome::openathan_upgrade_qualification {
bool allowed_url(const std::string &url) {
  return url.size() <= 4096 && url.rfind(OPENATHAN_QUALIFICATION_ORIGIN "/", 0) == 0;
}
void configure_tls(esp_http_client_config_t &config) { config.cert_pem = OPENATHAN_QUALIFICATION_CA; }
bool Qualification::services_ready(bool confirmed, const std::string &action) const {
  // USB power cannot establish Pyramid health; only descriptor reads bypass it.
  return confirmed || action == "check";
}
bool Qualification::startup_health(bool health) const { return health && !OPENATHAN_QUALIFICATION_STARTUP_FAILURE; }
bool Qualification::prepare_baseline(openathan_device::Upgrade &upgrade) {
  if (!upgrade.bootloader_ok_) return false;
  const auto *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state{};
  if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
    // Initialize only wholly erased metadata using IDF. Never repair corruption.
    std::array<uint8_t, 256> data{};
    bool erased = true;
    for (uint32_t offset = 0xe000; erased && offset < 0x10000; offset += data.size()) {
      erased = esp_flash_read(esp_flash_default_chip, data.data(), offset, data.size()) == ESP_OK;
      for (auto byte : data) erased = erased && byte == 0xff;
    }
    if (!erased || esp_ota_set_boot_partition(running) != ESP_OK) {
      upgrade.storage_ok_ = false; upgrade.error_ = "Qualification baseline OTA metadata is invalid"; return false;
    }
    App.safe_reboot(); return false;
  }
  if (state != ESP_OTA_IMG_VALID && state != ESP_OTA_IMG_UNDEFINED && state != ESP_OTA_IMG_PENDING_VERIFY) {
    upgrade.storage_ok_ = false; upgrade.error_ = "Qualification baseline is not bootable"; return false;
  }
  return state != ESP_OTA_IMG_UNDEFINED || esp_ota_mark_app_valid_cancel_rollback() == ESP_OK;
}
void Qualification::worker_finished(uint32_t free_bytes) {
  // One worker writes; snapshots can read concurrently. ESP-IDF reports bytes.
  if (free_bytes < worker_stack_free_min_.load()) worker_stack_free_min_ = free_bytes;
}
void Qualification::snapshot(JsonObject root) {
  auto q = root["qualification"].to<JsonObject>();
  q["identity"] = "OPENATHAN_QUALIFICATION_V1";
  q["origin"] = OPENATHAN_QUALIFICATION_ORIGIN;
  q["phase"] = phase_; q["armed"] = arm_;
  q["startup_failure"] = OPENATHAN_QUALIFICATION_STARTUP_FAILURE;
  q["erase_attempts"] = erase_attempts_.load(); q["write_attempts"] = write_attempts_.load();
  q["finalize_attempts"] = finalize_attempts_.load(); q["selection_attempts"] = selection_attempts_.load();
  const auto *running = esp_ota_get_running_partition(), *selected = esp_ota_get_boot_partition();
  esp_ota_img_states_t state{};
  q["running_offset"] = running ? running->address : 0;
  q["selected_offset"] = selected ? selected->address : 0;
  q["ota_state"] = running && esp_ota_get_state_partition(running, &state) == ESP_OK ? int(state) : -1;
  q["free_internal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["largest_internal"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["min_internal"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["free_psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  q["largest_psram"] = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  q["reset_reason"] = int(esp_reset_reason());
  const auto stack_free = worker_stack_free_min_.load();
  if (stack_free != UINT32_MAX) q["worker_stack_min_free"] = stack_free;
}
int Qualification::action(openathan_device::Upgrade &upgrade, JsonObjectConst input, std::string &error) {
  if (input.size() == 2 && input["command"].as<std::string>() == "arm" && input["phase"].is<const char *>()) {
    const auto phase = input["phase"].as<std::string>();
    if (phase != "before_boot_selection" && phase != "after_boot_selection") {
      error = "Unknown qualification phase"; return 400;
    }
    if (upgrade.active_ || !upgrade.queued_.envelope.empty() || !arm_.empty()) {
      error = "Arm a hold before requesting an update"; return 409;
    }
    arm_ = phase; released_ = false; return 200;
  }
  if (input.size() == 1 && input["command"].as<std::string>() == "release" && !arm_.empty()) {
    released_ = true; return 200;
  }
  error = "Invalid qualification command"; return 400;
}
void Qualification::cancelled() { arm_.clear(); released_ = false; phase_ = "idle"; }
bool Qualification::hold_(const char *phase) {
  phase_ = phase;
  if (arm_ != phase) return false;
  if (!released_) return true;
  arm_.clear(); released_ = false; return false;
}
bool Qualification::selected_loop(bool safe) {
  if (!selected_) return false;
  if (!hold_("after_boot_selection") && safe) App.safe_reboot();
  return true;
}
bool Qualification::should_reconcile(const openathan_device::Upgrade &upgrade) const { return !upgrade.staged_; }
bool Qualification::persist_handoff(openathan_device::Upgrade &upgrade) {
  // Repeated test pauses must not rewrite the durable request. Production has
  // no pauses and retains its original persistence/reconciliation behavior.
  return upgrade.expected_ == upgrade.queued_.version || upgrade.persist_(upgrade.queued_.envelope, upgrade.queued_.version);
}
bool Qualification::before_selection() {
  if (hold_("before_boot_selection")) return true;
  ++selection_attempts_; return false;
}
bool Qualification::after_selection() { selected_ = true; return hold_("after_boot_selection"); }
}  // namespace esphome::openathan_upgrade_qualification
