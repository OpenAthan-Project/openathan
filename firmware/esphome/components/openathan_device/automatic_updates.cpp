#include "upgrade.h"

namespace esphome::openathan_device {
std::string Upgrade::request_identity_(const UpgradeRelease &release) {
  return release.version + ":" + release.sha256;
}
void Upgrade::load_automatic_() {
  size_t size{};
  const auto result = nvs_get_blob(nvs_, "automatic", nullptr, &size);
  if (result == ESP_ERR_NVS_NOT_FOUND) return;
  automatic_ok_ = false;
  if (result != ESP_OK || !size || size > 512) return;
  std::string record(size, '\0');
  JsonDocument doc;
  if (nvs_get_blob(nvs_, "automatic", record.data(), &size) != ESP_OK || deserializeJson(doc, record) ||
      !doc.is<JsonObject>() || doc.size() != 5 || !doc["schema"].is<unsigned>() || doc["schema"].as<unsigned>() != 1 ||
      !doc["enabled"].is<bool>() || !doc["revision"].is<uint32_t>() ||
      !doc["skipped"].is<const char *>() || !doc["request"].is<const char *>()) return;
  const auto skipped = doc["skipped"].as<std::string>(), request = doc["request"].as<std::string>();
  if (!skipped.empty() && !release_version(skipped)) return;
  if (!request.empty()) {
    const auto colon = request.find(':');
    if (colon == std::string::npos || !release_version(std::string_view(request).substr(0, colon)) ||
        request.size() - colon - 1 != 64 || request.find_first_not_of("0123456789abcdef", colon + 1) != std::string::npos) return;
  }
  automatic_ = {doc["enabled"].as<bool>(), doc["revision"].as<uint32_t>(), skipped, request};
  automatic_ok_ = true;
}
bool Upgrade::save_automatic_(bool enabled, uint32_t revision, const std::string &skipped, const std::string &request) {
  if (!automatic_ok_) return false;
  if (enabled == automatic_.enabled && revision == automatic_.revision && skipped == automatic_.skipped &&
      request == automatic_.request) return true;
  JsonDocument doc;
  doc["schema"] = 1; doc["enabled"] = enabled; doc["revision"] = revision;
  doc["skipped"] = skipped; doc["request"] = request;
  std::string record;
  serializeJson(doc, record);
  if (doc.overflowed() || nvs_set_blob(nvs_, "automatic", record.data(), record.size()) != ESP_OK || nvs_commit(nvs_) != ESP_OK) {
    automatic_ok_ = false;
    cancel_ = true;
    return false;
  }
  automatic_ = {enabled, revision, skipped, request};
  return true;
}
bool Upgrade::suppress_(const std::string &version) {
  const auto skipped = automatic_.skipped.empty() || newer_release(version, automatic_.skipped) ? version : automatic_.skipped;
  const auto request = !usb_request_ && !queued_.envelope.empty() ? request_identity_(queued_) : automatic_.request;
  return save_automatic_(automatic_.enabled, automatic_.revision, skipped, request);
}
bool Upgrade::cancel_request_() {
  cancel_ = true;
  if (!save_verify_hold_("") || !persist_("", "")) return false;
  queued_ = {}; staged_ = nullptr; expected_.clear(); automatic_request_ = false;
  usb_request_ = false; handoff_state_ = Handoff::UNKNOWN;
  state_ = "idle"; error_.clear();
  return true;
}
int Upgrade::change_automatic_(JsonObjectConst input, std::string &error) {
  if (input.size() != 3 || !input["schema"].is<unsigned>() || input["schema"].as<unsigned>() != 1 ||
      !input["enabled"].is<bool>() || !input["expected_revision"].is<uint32_t>()) {
    error = "Invalid automatic update preference"; return 400;
  }
  if (!automatic_ok_) { error = "Automatic update storage is unavailable; restart the speaker"; return 503; }
  if (input["expected_revision"].as<uint32_t>() != automatic_.revision) {
    error = "Reload automatic updates before saving"; return 409;
  }
  if (state_ == "restarting") { error = "The update is already restarting"; return 409; }
  const bool enabled = input["enabled"].as<bool>();
  if (enabled != automatic_.enabled) {
    if (automatic_.revision == UINT32_MAX ||
        !save_automatic_(enabled, automatic_.revision + 1, automatic_.skipped, automatic_.request)) {
      error = "Automatic updates could not be saved; restart the speaker"; return 503;
    }
  }
  // Commit the opt-out before clearing its request. A cut between the commits
  // leaves a recognizable automatic request that the next boot will cancel.
  if (!enabled && automatic_request_ && !cancel_request_()) {
    error = "The update could not be canceled; restart the speaker"; return 503;
  }
  return 200;
}
void Upgrade::automatic_loop_() {
  if (!automatic_ok_ || !verify_ok_ || !verify_hold_.empty() || !automatic_.enabled || !bootloader_ok_ || !athan_->activated() || !safe_ ||
      offered_.envelope.empty() || (!automatic_.skipped.empty() && !newer_release(offered_.version, automatic_.skipped))) return;
  // The source marker precedes the backward-readable request. An orphan marker
  // authorizes nothing; only an exact matching, verified request is automatic.
  if (!save_automatic_(automatic_.enabled, automatic_.revision, automatic_.skipped, request_identity_(offered_)) ||
      !persist_(offered_.envelope, "")) return;
  queued_ = offered_; automatic_request_ = true; state_ = "queued"; error_.clear(); retry_ms_ = 0;
}
void Upgrade::reject_(const char *message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cancel_) return;
  if (!suppress_(queued_.version) || !cancel_request_()) {
    state_ = "failed"; error_ = "Rejected firmware could not be recorded; restart the speaker"; return;
  }
  state_ = "failed"; error_ = message;
}
}  // namespace esphome::openathan_device
