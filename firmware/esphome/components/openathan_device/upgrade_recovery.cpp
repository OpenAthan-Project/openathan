#include "upgrade.h"
#include <bootloader_common.h>
#include <esp_flash_partitions.h>
#include <esp_ota_ops.h>
#include <algorithm>
#include <array>

namespace esphome::openathan_device {
std::string Upgrade::identity_version_(const std::string &identity) {
  const auto colon = identity.find(':');
  if (colon == std::string::npos || !release_version(std::string_view(identity).substr(0, colon)) ||
      identity.size() - colon - 1 != 64 ||
      identity.find_first_not_of("0123456789abcdef", colon + 1) != std::string::npos) return {};
  return identity.substr(0, colon);
}
void Upgrade::load_verify_hold_() {
  size_t size{};
  const auto result = nvs_get_blob(nvs_, "verify_hold", nullptr, &size);
  if (result == ESP_ERR_NVS_NOT_FOUND || (result == ESP_OK && size == 0)) return;
  verify_ok_ = false;
  if (result != ESP_OK || size > 256) return;
  std::string record(size, '\0');
  JsonDocument doc;
  if (nvs_get_blob(nvs_, "verify_hold", record.data(), &size) != ESP_OK || deserializeJson(doc, record) ||
      !doc.is<JsonObject>() || doc.size() != 2 || !doc["schema"].is<unsigned>() ||
      doc["schema"].as<unsigned>() != 1 || !doc["identity"].is<const char *>()) return;
  const auto identity = doc["identity"].as<std::string>();
  if (!identity.empty() && identity_version_(identity).empty()) return;
  verify_hold_ = identity;
  verify_ok_ = true;
}
bool Upgrade::save_verify_hold_(const std::string &identity) {
  if (!storage_ok_ || !verify_ok_) return false;
  if (identity == verify_hold_) return true;
  JsonDocument doc;
  doc["schema"] = 1; doc["identity"] = identity;
  std::string record;
  serializeJson(doc, record);
  if (doc.overflowed() || nvs_set_blob(nvs_, "verify_hold", record.data(), record.size()) != ESP_OK ||
      nvs_commit(nvs_) != ESP_OK) {
    verify_ok_ = false; cancel_ = true;
    error_ = "Firmware verification storage is unavailable; restart the speaker";
    return false;
  }
  verify_hold_ = identity;
  return true;
}
bool Upgrade::recover_verification_() {
  if (!verify_ok_) {
    error_ = "Firmware verification storage is unavailable; restart the speaker";
    return false;
  }
  if (verify_hold_.empty()) return true;
  const auto version = identity_version_(verify_hold_);
  if (queued_.envelope.empty()) {
    if (!newer_release(version, OPENATHAN_FIRMWARE_VERSION) ||
        (automatic_ok_ && !automatic_.skipped.empty() && !newer_release(version, automatic_.skipped)))
      return save_verify_hold_("");
    state_ = "failed";
    error_ = "Interrupted firmware verification must be canceled before another installation";
    return false;
  }
  if (verify_hold_ != request_identity_(queued_)) {
    state_ = "failed";
    error_ = "Firmware verification does not match its saved request; restart the speaker";
    return false;
  }
  if (!suppress_(version) || !cancel_request_()) {
    state_ = "failed";
    error_ = "Interrupted verification could not be retired; restart the speaker";
    return false;
  }
  state_ = "failed";
  error_ = "Firmware verification was interrupted; check for updates and install the release manually";
  return true;
}
Upgrade::Handoff Upgrade::handoff_() {
  const auto *running = esp_ota_get_running_partition();
  const auto *candidate = esp_ota_get_next_update_partition(nullptr);
  const auto &version = expected_.empty() ? queued_.version : expected_;
  if (!running || !candidate || running == candidate || version.empty() ||
      running->type != ESP_PARTITION_TYPE_APP || candidate->type != ESP_PARTITION_TYPE_APP ||
      esp_ota_get_app_partition_count() != 2) return Handoff::UNKNOWN;
  const unsigned running_slot = running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
  const unsigned candidate_slot = candidate->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
  if (running_slot > 1 || candidate_slot > 1 || running_slot == candidate_slot) return Handoff::UNKNOWN;
  esp_app_desc_t description{};
  const bool identified = esp_ota_get_partition_description(candidate, &description) == ESP_OK &&
      version == description.version;
  if (!expected_.empty() && !identified) return Handoff::UNKNOWN;
  esp_ota_img_states_t observed{};
  const auto state_result = identified ? esp_ota_get_state_partition(candidate, &observed) : ESP_ERR_NOT_FOUND;
  if (state_result != ESP_OK && state_result != ESP_ERR_NOT_FOUND) return Handoff::UNKNOWN;
  const auto *data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!data || data->erase_size < sizeof(esp_ota_select_entry_t) || data->erase_size > data->size / 2)
    return Handoff::UNKNOWN;
  std::array<esp_ota_select_entry_t, 2> entries{};
  for (size_t index = 0; index < entries.size(); ++index)
    if (esp_partition_read(data, index * data->erase_size, &entries[index], sizeof(entries[index])) != ESP_OK)
      return Handoff::UNKNOWN;
  const esp_ota_select_entry_t *current = nullptr, *next = nullptr, *selected = nullptr;
  for (const auto &entry : entries) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&entry);
    if (std::all_of(bytes, bytes + sizeof(entry), [](uint8_t byte) { return byte == 0xff; })) continue;
    if (entry.ota_seq == 0 || entry.ota_seq == UINT32_MAX ||
        entry.crc != bootloader_common_ota_select_crc(&entry) ||
        (entry.ota_state > ESP_OTA_IMG_ABORTED && entry.ota_state != ESP_OTA_IMG_UNDEFINED)) return Handoff::UNKNOWN;
    const unsigned slot = (entry.ota_seq - 1) % 2;
    const auto **target = slot == running_slot ? &current : &next;
    if (*target) return Handoff::UNKNOWN;
    *target = &entry;
    if (entry.ota_state != ESP_OTA_IMG_INVALID && entry.ota_state != ESP_OTA_IMG_ABORTED &&
        (!selected || entry.ota_seq > selected->ota_seq)) selected = &entry;
  }
  const auto *boot = esp_ota_get_boot_partition();
  if (!boot || !current || (boot != running && boot != candidate)) return Handoff::UNKNOWN;
  if (boot == candidate) {
    if (!identified || !next || selected != next || state_result != ESP_OK || observed != next->ota_state)
      return Handoff::UNKNOWN;
    return Handoff::SELECTED;
  }
  if (selected != current || (current->ota_state != ESP_OTA_IMG_VALID && current->ota_state != ESP_OTA_IMG_UNDEFINED))
    return Handoff::UNKNOWN;
  if (next && (next->ota_state == ESP_OTA_IMG_INVALID || next->ota_state == ESP_OTA_IMG_ABORTED)) {
    if (!expected_.empty() && identified && (state_result == ESP_ERR_NOT_FOUND || observed == next->ota_state))
      return Handoff::REJECTED;
    return expected_.empty() ? Handoff::INTERRUPTED : Handoff::UNKNOWN;
  }
  if (next && state_result == ESP_OK && observed != next->ota_state) return Handoff::UNKNOWN;
  if (!next && state_result == ESP_OK && observed != ESP_OTA_IMG_UNDEFINED) return Handoff::UNKNOWN;
  return Handoff::INTERRUPTED;
}
}  // namespace esphome::openathan_device
