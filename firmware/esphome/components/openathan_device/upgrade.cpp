#include "upgrade.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <array>
#include <ctime>

namespace esphome::openathan_device {
namespace {
constexpr const char *BASE = "https://github.com/OpenAthan-Project/openathan/releases/";
uint64_t milliseconds() { return esp_timer_get_time() / 1000; }
bool hex(const std::string &value, uint8_t *output, size_t size) {
  if (value.size() != size * 2) return false;
  auto digit = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (size_t i = 0; i < size; ++i) {
    const int a = digit(value[i * 2]), b = digit(value[i * 2 + 1]);
    if (a < 0 || b < 0) return false;
    output[i] = a * 16 + b;
  }
  return true;
}
bool allowed_url(const std::string &url) {
  // Redirects are necessary for GitHub release storage. Never send device
  // credentials, and never follow an insecure or arbitrary-host redirect.
  for (const char *host : {"https://github.com/", "https://release-assets.githubusercontent.com/",
                           "https://objects.githubusercontent.com/"})
    if (url.rfind(host, 0) == 0 && url.size() <= 4096) return true;
  return false;
}
struct Response {
  esp_http_client_handle_t client{};
  std::string location;
  static esp_err_t event(esp_http_client_event_t *event) {
    auto *response = static_cast<Response *>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_HEADER && strcasecmp(event->header_key, "Location") == 0)
      response->location = event->header_value;
    return ESP_OK;
  }
  ~Response() { if (client) esp_http_client_cleanup(client); }
  bool open(std::string url) {
    for (unsigned redirects = 0; redirects <= 4; ++redirects) {
      if (!allowed_url(url)) return false;
      esp_http_client_config_t config{};
      config.url = url.c_str();
      config.crt_bundle_attach = esp_crt_bundle_attach;
      config.timeout_ms = 4000;
      config.disable_auto_redirect = true;
      config.buffer_size = 4096;
      config.buffer_size_tx = 512;
      config.event_handler = event;
      config.user_data = this;
      location.clear();
      client = esp_http_client_init(&config);
      if (!client || esp_http_client_open(client, 0) != ESP_OK || esp_http_client_fetch_headers(client) < 0) return false;
      const int status = esp_http_client_get_status_code(client);
      if (status == 200) return true;
      if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308) return false;
      if (location.empty()) return false;
      url = location;
      esp_http_client_cleanup(client);
      client = nullptr;
    }
    return false;
  }
};
}  // namespace
bool Upgrade::descriptor_(const std::string &envelope, UpgradeRelease &release) {
  if (envelope.empty() || envelope.size() > 8192) return false;
  JsonDocument wrapper;
  if (deserializeJson(wrapper, envelope) || !wrapper.is<JsonObject>() || wrapper.size() != 2 ||
      !wrapper["payload"].is<const char *>() || !wrapper["signature"].is<const char *>()) return false;
  const auto payload = wrapper["payload"].as<std::string>();
  const auto signature = wrapper["signature"].as<std::string>();
  if (payload.size() > 4096 || signature.size() < 128 || signature.size() > 144 || signature.size() % 2) return false;
  std::array<uint8_t, 72> sig{};
  uint8_t hash[32];
  if (!hex(signature, sig.data(), signature.size() / 2) ||
      mbedtls_sha256(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), hash, 0) != 0) return false;
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  const char *pem = OPENATHAN_UPGRADE_PUBLIC_KEY;
  const bool verified = mbedtls_pk_parse_public_key(&key, reinterpret_cast<const uint8_t *>(pem), strlen(pem) + 1) == 0 &&
      mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig.data(), signature.size() / 2) == 0;
  mbedtls_pk_free(&key);
  if (!verified) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>() || doc.size() != 10 ||
      !doc["schema"].is<unsigned>() || doc["schema"].as<unsigned>() != 1 ||
      doc["hardware"].as<std::string>() != "atoms3r-c126-pyramid-a167" ||
      doc["layout"].as<std::string>() != "dual-2m-audio-3_5m-v1" ||
      !doc["storageFormat"].is<unsigned>() || doc["storageFormat"].as<unsigned>() != 1 ||
      !doc["audioFormat"].is<unsigned>() || doc["audioFormat"].as<unsigned>() != 1 ||
      !doc["rollback"].is<bool>() || !doc["rollback"].as<bool>() ||
      !doc["bytes"].is<uint32_t>() || doc["bytes"].as<uint32_t>() < 256 || doc["bytes"].as<uint32_t>() > 1572864) return false;
  release.version = doc["version"].as<std::string>();
  release.commit = doc["commit"].as<std::string>();
  release.sha256 = doc["sha256"].as<std::string>();
  uint8_t ignored[32];
  if (!release_version(release.version) || !hex(release.sha256, ignored, 32) || !hex(release.commit, ignored, 20)) return false;
  release.bytes = doc["bytes"].as<uint32_t>();
  release.envelope = envelope;
  return true;
}
bool Upgrade::persist_(const std::string &queue, const std::string &expected) {
  if (!storage_ok_) return false;
  JsonDocument doc;
  doc["schema"] = 1; doc["queue"] = queue; doc["expected"] = expected;
  std::string record;
  serializeJson(doc, record);
  if (nvs_set_blob(nvs_, "request", record.data(), record.size()) != ESP_OK || nvs_commit(nvs_) != ESP_OK) {
    storage_ok_ = false;
    return false;
  }
  return true;
}
void Upgrade::begin(openathan_component::OpenAthan *athan, bool server_ready) {
  athan_ = athan; server_ready_ = server_ready;
  if (!server_ready_) {
    esp_ota_img_states_t state{};
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) esp_ota_mark_app_invalid_rollback_and_reboot();
  }
  // A new application cannot retrofit rollback into an old bootloader. Hash
  // the fixed bootloader region; never infer support from dual app slots.
  mbedtls_sha256_context boot_hash;
  mbedtls_sha256_init(&boot_hash);
  bool readable = mbedtls_sha256_starts(&boot_hash, 0) == 0;
  std::array<uint8_t, 1024> block{};
  for (uint32_t offset = 0; readable && offset < 0x8000; offset += block.size())
    readable = esp_flash_read(esp_flash_default_chip, block.data(), offset, block.size()) == ESP_OK &&
        mbedtls_sha256_update(&boot_hash, block.data(), block.size()) == 0;
  uint8_t digest[32];
  readable = readable && mbedtls_sha256_finish(&boot_hash, digest) == 0;
  mbedtls_sha256_free(&boot_hash);
  std::string hash;
  if (readable) for (auto byte : digest) { hash += "0123456789abcdef"[byte >> 4]; hash += "0123456789abcdef"[byte & 15]; }
  bootloader_ok_ = readable && std::string(OPENATHAN_ROLLBACK_BOOTLOADERS).find(hash) != std::string::npos;
  boot_ms_ = milliseconds(); next_check_ms_ = boot_ms_ + 30000 + esp_random() % 30000;
  const char *space = openathan_storage::TEST_MODE ? "oa_upgrade_test" : "oa_upgrade";
  storage_ok_ = nvs_open(space, NVS_READWRITE, &nvs_) == ESP_OK;
  if (!storage_ok_) return;
  size_t size = 0;
  const auto result = nvs_get_blob(nvs_, "request", nullptr, &size);
  if (result == ESP_ERR_NVS_NOT_FOUND) return;
  if (result != ESP_OK || size == 0 || size > 10000) { storage_ok_ = false; return; }
  std::string record(size, '\0');
  JsonDocument doc;
  if (nvs_get_blob(nvs_, "request", record.data(), &size) != ESP_OK || deserializeJson(doc, record) ||
      doc.size() != 3 || doc["schema"].as<unsigned>() != 1 ||
      !doc["queue"].is<const char *>() || !doc["expected"].is<const char *>()) { storage_ok_ = false; return; }
  expected_ = doc["expected"].as<std::string>();
  if (!expected_.empty() && !release_version(expected_)) { storage_ok_ = false; return; }
  const auto queue = doc["queue"].as<std::string>();
  if (!queue.empty()) {
    if (!descriptor_(queue, queued_) || (!expected_.empty() && queued_.version != expected_)) {
      storage_ok_ = false; return;
    }
    state_ = "queued";
  }
}
void Upgrade::fail_(const char *message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (cancel_) return;
  state_ = queued_.envelope.empty() ? "failed" : "queued";
  error_ = message;
  retry_ms_ = milliseconds() + 300000;
}
void Upgrade::snapshot(JsonObject root) {
  std::lock_guard<std::mutex> lock(mutex_);
  root["version"] = OPENATHAN_FIRMWARE_VERSION;
  root["supported"] = bootloader_ok_;
  root["commit"] = OPENATHAN_BUILD_COMMIT;
  root["state"] = storage_ok_ ? state_ : "storage_fault";
  root["error"] = error_;
  root["result"] = result_;
  root["last_check"] = last_check_;
  root["received"] = received_.load();
  root["total"] = queued_.bytes;
  root["queued_version"] = queued_.version;
  if (!offered_.version.empty()) {
    root["available"]["version"] = offered_.version;
    root["available"]["bytes"] = offered_.bytes;
    root["available"]["notes"] = std::string(BASE) + "tag/" + offered_.version;
  }
}
int Upgrade::action(const std::string &action, JsonObjectConst input, std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!storage_ok_ || !confirmed_) { error = "Firmware upgrade services are not ready"; return 503; }
  if (action == "cancel" && input.size() == 0) {
    if (state_ == "restarting") { error = "The update is already restarting"; return 409; }
    if (!persist_("", "")) { error = "Update request could not be saved; restart the device"; return 503; }
    cancel_ = true; queued_ = {}; staged_ = nullptr; expected_.clear();
    state_ = "idle"; error_.clear();
    return 200;
  }
  if (active_ || !queued_.envelope.empty()) { error = "An update is already in progress"; return 409; }
  if (action == "check" && input.size() == 0) {
    if (!start_(false)) { error = "Unable to start checking for updates"; return 503; }
    return 200;
  }
  if (action == "install" && input.size() == 1 && input["version"].is<const char *>()) {
    if (openathan_storage::TEST_MODE) { error = "Isolated test firmware cannot install production releases"; return 503; }
    if (!bootloader_ok_) { error = "This speaker needs a maintainer USB bootloader transition before Wi-Fi updates"; return 503; }
    if (offered_.version.empty() || input["version"].as<std::string>() != offered_.version) {
      error = "Check for updates and review the available version"; return 409;
    }
    if (!persist_(offered_.envelope, "")) { error = "Update request could not be saved; restart the device"; return 503; }
    queued_ = offered_; state_ = "queued"; error_.clear(); retry_ms_ = 0;
    return 200;
  }
  error = "Invalid update request";
  return 400;
}
bool Upgrade::start_(bool install) {
  if (active_) return false;
  active_ = true; cancel_ = false; install_job_ = install; received_ = 0;
  state_ = install ? "downloading" : "checking"; error_.clear();
  if (xTaskCreate(worker_, "oa_upgrade", 8192, this, 1, nullptr) != pdPASS) {
    active_ = false; state_ = install ? "queued" : "failed";
    error_ = "Not enough memory to start the update; retrying after five minutes";
    retry_ms_ = milliseconds() + 300000;
    if (!install) next_check_ms_ = retry_ms_;
    return false;
  }
  return true;
}
void Upgrade::worker_(void *argument) {
  auto *self = static_cast<Upgrade *>(argument);
  self->run_(); self->active_ = false;
  vTaskDelete(nullptr);
}
void Upgrade::run_() {
  if (!install_job_) {
    Response response;
    if (!response.open(std::string(BASE) + "latest/download/upgrade.json")) { fail_("Could not check for updates; try again later"); return; }
    std::string envelope;
    char buffer[1024];
    const auto deadline = milliseconds() + 30000;
    while (!cancel_ && milliseconds() < deadline) {
      const int count = esp_http_client_read(response.client, buffer, sizeof(buffer));
      if (count < 0 || envelope.size() + count > 8192) { fail_("Invalid update response"); return; }
      if (count == 0) break;
      envelope.append(buffer, count);
    }
    if (cancel_) return;
    UpgradeRelease release;
    if (!esp_http_client_is_complete_data_received(response.client) || !descriptor_(envelope, release)) {
      fail_("The release could not be verified or is incompatible"); return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (cancel_) return;
    offered_ = newer_release(release.version, OPENATHAN_FIRMWARE_VERSION) ? release : UpgradeRelease{};
    last_check_ = ::time(nullptr);
    state_ = offered_.version.empty() ? "current" : "available";
    return;
  }
  UpgradeRelease release;
  { std::lock_guard<std::mutex> lock(mutex_); release = queued_; }
  if (cancel_) return;
  if (!safe_) { fail_("Waiting for a safe time between prayers"); return; }
  Response response;
  if (!response.open(std::string(BASE) + "download/" + release.version + "/firmware.ota.bin") ||
      esp_http_client_get_content_length(response.client) != release.bytes) { fail_("Could not download the verified firmware"); return; }
  const auto *partition = esp_ota_get_next_update_partition(nullptr);
  esp_ota_handle_t handle = 0;
  // HTTP open/header processing can block while the main loop publishes a
  // new playback/cancellation state. esp_ota_begin may erase the entire image.
  if (cancel_) return;
  if (!safe_) { fail_("Waiting for a safe time between prayers"); return; }
  if (!partition || partition->size != 0x200000 || esp_ota_begin(partition, release.bytes, &handle) != ESP_OK) {
    // Pinned IDF allocates the handle before erasing. An erase failure leaves
    // it live, whereas failures before allocation leave our zero sentinel.
    if (handle) esp_ota_abort(handle);
    fail_("The inactive application slot is unavailable"); return;
  }
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash); mbedtls_sha256_starts(&hash, 0);
  std::array<uint8_t, 4096> buffer{};
  uint32_t total = 0;
  bool ok = true;
  const auto deadline = milliseconds() + 180000;
  while (total < release.bytes && ok && !cancel_ && safe_ && milliseconds() < deadline) {
    const int count = esp_http_client_read(response.client, reinterpret_cast<char *>(buffer.data()),
        std::min<size_t>(buffer.size(), release.bytes - total));
    if (count <= 0 || mbedtls_sha256_update(&hash, buffer.data(), count) != 0) { ok = false; break; }
    // Never use the safety sample from before a blocking read to authorize a
    // flash write. Cancellation also stops a successfully received chunk.
    if (cancel_ || !safe_ || esp_ota_write(handle, buffer.data(), count) != ESP_OK) { ok = false; break; }
    total += count; received_ = total;
    vTaskDelay(1);
  }
  uint8_t actual[32], expected[32];
  ok = ok && total == release.bytes && esp_http_client_is_complete_data_received(response.client) &&
      mbedtls_sha256_finish(&hash, actual) == 0 && hex(release.sha256, expected, 32) && memcmp(actual, expected, 32) == 0;
  mbedtls_sha256_free(&hash);
  if (!ok || cancel_ || !safe_) { esp_ota_abort(handle); if (!cancel_) fail_("Download paused; waiting for a safe time or a retry"); return; }
  { std::lock_guard<std::mutex> lock(mutex_); if (!cancel_) state_ = "verifying"; }
  if (cancel_ || !safe_) { esp_ota_abort(handle); if (!cancel_) fail_("Waiting for a safe time between prayers"); return; }
  if (esp_ota_end(handle) != ESP_OK) { fail_("The firmware image failed validation"); return; }
  esp_app_desc_t description{};
  if (esp_ota_get_partition_description(partition, &description) != ESP_OK || release.version != description.version) {
    fail_("The firmware version does not match the verified release"); return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!cancel_) { staged_ = partition; state_ = "queued"; }
}
void Upgrade::loop(bool connected) {
  const auto now = milliseconds();
  const auto status = athan_->status();
  const auto clock = athan_->read();
  const auto *settings = athan_->settings_service();
  const bool health = server_ready_ && !athan_->is_failed() && settings && settings->healthy() &&
      std::string(athan_->setup_state()) != "storage_fault" && athan_->upgrade_health() &&
      boot_scheduler_healthy(status.fault);
  safe_ = upgrade_window(status.playing, clock.valid, athan_->activated(),
      health && status.automatic_ready, clock.utc, status.next ? std::optional<int64_t>(status.next->utc) : std::nullopt);
  std::lock_guard<std::mutex> lock(mutex_);
  if (!confirmed_) {
    if (now - boot_ms_ < 30000) return;
    esp_ota_img_states_t image_state{};
    const bool pending = esp_ota_get_state_partition(esp_ota_get_running_partition(), &image_state) == ESP_OK && image_state == ESP_OTA_IMG_PENDING_VERIFY;
    if (!health || !storage_ok_) {
      if (pending && now - boot_ms_ >= 90000) esp_ota_mark_app_invalid_rollback_and_reboot();
      return;
    }
    if (pending && esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) return;
    confirmed_ = true;
  }
  if (!storage_ok_ || active_) return;
  if (!queued_.envelope.empty() && !newer_release(queued_.version, OPENATHAN_FIRMWARE_VERSION)) {
    // A preserving USB update can fulfill or supersede a queued Wi-Fi request
    // without setting our handoff marker. A valid old request is not corruption.
    const bool fulfilled = queued_.version == OPENATHAN_FIRMWARE_VERSION;
    result_ = fulfilled ? "success" : "superseded";
    state_ = fulfilled ? "success" : "current";
    if (persist_("", "")) { expected_.clear(); queued_ = {}; }
  }
  if (!storage_ok_) return;
  if (!expected_.empty()) {
    const auto *candidate = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t description{};
    esp_ota_img_states_t candidate_state{};
    const bool identified = candidate && esp_ota_get_partition_description(candidate, &description) == ESP_OK &&
        expected_ == description.version;
    const bool rejected = identified && esp_ota_get_state_partition(candidate, &candidate_state) == ESP_OK &&
        (candidate_state == ESP_OTA_IMG_INVALID || candidate_state == ESP_OTA_IMG_ABORTED);
    if (expected_ == OPENATHAN_FIRMWARE_VERSION || rejected) {
      result_ = expected_ == OPENATHAN_FIRMWARE_VERSION ? "success" : "rolled_back";
      state_ = result_;
      if (persist_("", "")) { expected_.clear(); queued_ = {}; }
    } else if (identified && esp_ota_get_boot_partition() == candidate) {
      // Selection completed, but this application is still running. Do not
      // erase either slot or rewrite boot metadata while handoff is pending.
      state_ = "restarting";
      if (safe_) App.safe_reboot();
      return;
    } else {
      // OTA begin invalidates the inactive slot's previous rollback metadata.
      // Without a rejected candidate or a selected slot, this is an interrupted
      // handoff, not evidence that the requested application failed startup.
      if (persist_(queued_.envelope, "")) expected_.clear();
      state_ = queued_.envelope.empty() ? "failed" : "queued";
      if (queued_.envelope.empty()) error_ = "Update handoff interrupted; check for updates again";
    }
  }
  if (!storage_ok_) return;
  if (openathan_storage::TEST_MODE) return;
  if (!queued_.envelope.empty() && !bootloader_ok_) {
    error_ = "This speaker needs a maintainer USB bootloader transition before Wi-Fi updates";
    return;
  }
  if (staged_ && safe_) {
    if (!persist_(queued_.envelope, queued_.version)) { error_ = "Update request storage failed; restart the device"; return; }
    expected_ = queued_.version;
    if (esp_ota_set_boot_partition(staged_) != ESP_OK) {
      persist_(queued_.envelope, ""); expected_.clear(); staged_ = nullptr;
      state_ = "queued"; error_ = "Could not select the new application"; retry_ms_ = now + 300000; return;
    }
    state_ = "restarting";
    // Boot selection and restart happen together after the final window check.
    App.safe_reboot();
    return;
  }
  if (staged_) return;
  if (!queued_.envelope.empty()) {
    if (safe_ && connected && now >= retry_ms_) start_(true);
    return;
  }
  if (connected && clock.valid && now >= next_check_ms_) {
    next_check_ms_ = now + 86400000 + esp_random() % 3600000;
    start_(false);
  }
}
}  // namespace esphome::openathan_device
