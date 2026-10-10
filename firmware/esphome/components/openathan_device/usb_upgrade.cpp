#include "upgrade.h"
#include "esphome/components/openathan/storage_config.h"
#include <esp_random.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <algorithm>
#include <array>
#include <charconv>

namespace esphome::openathan_device {
namespace {
uint64_t usb_now() { return esp_timer_get_time() / 1000; }
std::string token_text(uint64_t token) {
  std::string text(16, '0');
  for (unsigned i = 0; i < 16; ++i) text[15-i] = "0123456789abcdef"[(token >> (4*i)) & 15];
  return text;
}
}
Upgrade::~Upgrade() {
  if (usb_handle_) esp_ota_abort(usb_handle_);
  if (usb_hash_live_) mbedtls_sha256_free(&usb_hash_);
}
bool Upgrade::usb_safe_() {
  if (!OPENATHAN_UPDATES_ENABLED || !confirmed_ || !storage_ok_ || !bootloader_ok_) return false;
  const auto status = athan_->status();
  const auto *settings = athan_->settings_service();
  if (!server_ready_ || athan_->is_failed() || !settings || !settings->healthy() ||
      std::string(athan_->setup_state()) == "storage_fault" || !athan_->upgrade_health()) return false;
  if (status.playing || !boot_scheduler_healthy(status.fault)) return false;
  const auto clock = athan_->read();
  // With no clock, automatic prayer playback is blocked. USB needs no network.
  return !clock.valid || upgrade_window(false, true, athan_->activated(), status.automatic_ready,
      clock.utc, status.next ? std::optional<int64_t>(status.next->utc) : std::nullopt);
}
std::vector<std::string> Upgrade::usb_info() {
  std::lock_guard<std::mutex> lock(mutex_);
  return {"1", OPENATHAN_FIRMWARE_VERSION, OPENATHAN_BUILD_COMMIT,
      OPENATHAN_UPDATES_ENABLED && bootloader_ok_ ? "supported" : "unsupported", storage_ok_ ? state_ : "storage_fault",
      confirmed_ ? "confirmed" : "pending", result_, queued_.version, std::to_string(received_.load()), OPENATHAN_HARDWARE};
}
Upgrade::UsbSelection Upgrade::usb_selection_() {
  // Called with mutex_ held. Empty expected can also be a handoff marker that
  // an older firmware cleared; it is never proof that selection did not happen.
  const auto *running = esp_ota_get_running_partition();
  const auto *candidate = esp_ota_get_next_update_partition(nullptr);
  if (!running || !candidate || candidate == running || queued_.envelope.empty()) return UsbSelection::UNKNOWN;
  esp_app_desc_t description{};
  const bool identified = esp_ota_get_partition_description(candidate, &description) == ESP_OK &&
      queued_.version == description.version;
  esp_ota_img_states_t image_state{};
  const auto state_result = identified ? esp_ota_get_state_partition(candidate, &image_state) : ESP_ERR_NOT_FOUND;
  // Read selection last, including when the inactive OTA entry is absent.
  const auto *boot = esp_ota_get_boot_partition();
  if (!boot || (boot != running && boot != candidate)) return UsbSelection::UNKNOWN;
  if (boot == candidate) {
    if (!identified || state_result != ESP_OK || image_state == ESP_OTA_IMG_INVALID ||
        image_state == ESP_OTA_IMG_ABORTED) return UsbSelection::UNKNOWN;
    return UsbSelection::SELECTED;
  }
  if ((!expected_.empty() && !identified) || (state_result != ESP_OK && state_result != ESP_ERR_NOT_FOUND))
    return UsbSelection::UNKNOWN;
  if (!expected_.empty() && identified && state_result == ESP_OK &&
      (image_state == ESP_OTA_IMG_INVALID || image_state == ESP_OTA_IMG_ABORTED)) return UsbSelection::REJECTED;
  // OTA begin can leave no inactive entry, and a partial image may have no
  // readable description. The successful running-slot selection is the proof.
  return UsbSelection::UNSELECTED;
}
bool Upgrade::usb_abort_() {
  if (!expected_.empty() || state_ == "awaiting_power") return false;
  if (usb_handle_) { esp_ota_abort(usb_handle_); usb_handle_ = 0; }
  if (usb_hash_live_) { mbedtls_sha256_free(&usb_hash_); usb_hash_live_ = false; }
  usb_token_ = 0; std::string{}.swap(usb_descriptor_); usb_descriptor_bytes_ = 0; staged_ = nullptr;
  if (usb_request_ && !persist_("", "")) return false;
  usb_request_ = false; queued_ = {}; received_ = 0; state_ = "idle";
  return true;
}
uint8_t Upgrade::usb_command(uint8_t command, const std::vector<std::string> &fields,
                           std::vector<std::string> &reply) {
  using namespace ::openathan::usb_upgrade;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!OPENATHAN_UPDATES_ENABLED) return 2;
  if (command == BEGIN) {
#ifndef OPENATHAN_UPGRADE_QUALIFICATION
    if (openathan_storage::TEST_MODE) return 2;
#endif
    if (fields.size() != 1) return 1;
    uint32_t size = 0;
    const auto &text = fields[0];
    auto [end, error] = std::from_chars(text.data(), text.data()+text.size(), size);
    if (error != std::errc{} || end != text.data()+text.size() || !size || size > 8192) return 1;
    if (active_ || usb_busy() || !queued_.envelope.empty() || !usb_safe_()) return 255;
    // The capability is available only after this firmware's own health check.
    usb_token_ = (uint64_t(esp_random()) << 32) | esp_random();
    if (!usb_token_) usb_token_ = 1;
    usb_descriptor_bytes_ = size; usb_descriptor_.clear(); usb_descriptor_.reserve(size);
    usb_last_ms_ = usb_now(); received_ = 0; state_ = "usb_descriptor"; error_.clear();
    reply = {token_text(usb_token_)}; return 0;
  }
  if (command == ABORT && fields == std::vector<std::string>{"interrupted"} && usb_request_ && !usb_token_) {
    if (!confirmed_ || !storage_ok_ || !expected_.empty() || state_ != "usb_interrupted") return 255;
    const auto selection = usb_selection_();
    if (selection != UsbSelection::UNSELECTED) {
      state_ = selection == UsbSelection::SELECTED ? "awaiting_power" : "usb_selection_uncertain";
      return 255;
    }
    if (!usb_abort_()) return 255;
    reply = {"aborted"}; return 0;
  }
  if (fields.size() != 1 || !usb_token_ || fields[0] != token_text(usb_token_)) return 1;
  usb_last_ms_ = usb_now();
  if (command == ABORT) {
    if (!usb_abort_()) return 255;
    reply = {"aborted"}; return 0;
  }
  if (command == VERIFY) {
    if (state_ != "usb_descriptor" || usb_descriptor_.size() != usb_descriptor_bytes_) return 1;
    UpgradeRelease release;
    if (!descriptor_(usb_descriptor_, release) || !newer_release(release.version, OPENATHAN_FIRMWARE_VERSION)) {
      usb_abort_(); return 1;
    }
    if (!usb_safe_()) { usb_abort_(); return 255; }
    usb_request_ = true;
    if (!persist_(release.envelope, "")) { usb_abort_(); return 255; }
    queued_ = release;
    const auto *partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || partition->size != 0x200000 || esp_ota_begin(partition, release.bytes, &usb_handle_) != ESP_OK) {
      usb_abort_(); return 255;
    }
    mbedtls_sha256_init(&usb_hash_); usb_hash_live_ = true;
    if (mbedtls_sha256_starts(&usb_hash_, 0) != 0) { usb_abort_(); return 255; }
    std::string{}.swap(usb_descriptor_); state_ = "usb_receiving";
    reply = {"accepted", release.version}; return 0;
  }
  if (command == FINISH) {
    if (state_ != "usb_receiving" || received_ != queued_.bytes || !usb_safe_()) return 255;
    uint8_t digest[32];
    if (mbedtls_sha256_finish(&usb_hash_, digest) != 0) { usb_abort_(); return 255; }
    mbedtls_sha256_free(&usb_hash_); usb_hash_live_ = false;
    std::string actual;
    for (auto byte : digest) { actual += "0123456789abcdef"[byte >> 4]; actual += "0123456789abcdef"[byte & 15]; }
    if (actual != queued_.sha256) { usb_abort_(); return 1; }
    const auto handle = usb_handle_; usb_handle_ = 0;
    if (esp_ota_end(handle) != ESP_OK) { usb_abort_(); return 1; }
    const auto *partition = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t description{};
    if (!partition || esp_ota_get_partition_description(partition, &description) != ESP_OK ||
        queued_.version != description.version) { usb_abort_(); return 1; }
    // Persist the handoff before selecting. A lost reply is never permission to retry.
    if (!persist_(queued_.envelope, queued_.version)) return 255;
    expected_ = queued_.version;
    usb_token_ = 0;
    if (esp_ota_set_boot_partition(partition) != ESP_OK) {
      state_ = "usb_selection_uncertain"; return 255;
    }
    state_ = "awaiting_power";
    reply = {"verified", queued_.version}; return 0;
  }
  return 2;
}
bool Upgrade::usb_chunk(const ::openathan::usb_upgrade::Chunk &chunk) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!OPENATHAN_UPDATES_ENABLED || !usb_token_ || chunk.token != usb_token_) return false;
  if (chunk.kind == 0) {
    if (state_ != "usb_descriptor" || chunk.offset != usb_descriptor_.size() ||
        chunk.bytes.size() > usb_descriptor_bytes_ - usb_descriptor_.size()) return false;
    usb_descriptor_.append(reinterpret_cast<const char *>(chunk.bytes.data()), chunk.bytes.size());
  } else {
    if (state_ != "usb_receiving" || chunk.offset != received_ ||
        chunk.bytes.size() > queued_.bytes - received_ || !usb_safe_()) return false;
    // Keep the write buffer in internal stack RAM while flash disables the PSRAM cache.
    std::array<uint8_t, ::openathan::usb_upgrade::CHUNK> internal{};
    std::copy(chunk.bytes.begin(), chunk.bytes.end(), internal.begin());
    if (esp_ota_write(usb_handle_, internal.data(), chunk.bytes.size()) != ESP_OK ||
        mbedtls_sha256_update(&usb_hash_, internal.data(), chunk.bytes.size()) != 0) {
      usb_abort_(); return false;
    }
    received_ += chunk.bytes.size();
  }
  usb_last_ms_ = usb_now(); return true;
}
}  // namespace esphome::openathan_device
