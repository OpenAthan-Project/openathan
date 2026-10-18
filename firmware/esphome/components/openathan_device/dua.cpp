#include "dua.h"
#include "upgrade.h"
#include "release_http.h"
#include "esphome/core/application.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <array>
#include <memory>
#include <limits>

namespace esphome::openathan_device {
namespace {
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
bool unhex(const std::string &s, uint8_t *out, size_t size) {
  if (s.size() != size * 2) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (size_t i = 0; i < size; ++i) {
    const int a = digit(s[i * 2]), b = digit(s[i * 2 + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = a * 16 + b;
  }
  return true;
}
bool hash_matches(const uint8_t *data, size_t size, const uint8_t *expected) {
  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx);
  int status = mbedtls_sha256_starts(&ctx, 0);
  while (size && !status) {
    const auto n = std::min(size, size_t(4096));
    status = mbedtls_sha256_update(&ctx, data, n); data += n; size -= n; App.feed_wdt();
  }
  uint8_t hash[32];
  if (!status) status = mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  return !status && std::memcmp(hash, expected, 32) == 0;
}
void put(uint8_t *at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) at[i] = value >> (8 * i);
}
}
bool DuaMedia::verify_(const char *envelope, size_t size, uint32_t bytes, const uint8_t *digest, bool exact_release) {
  if (!size || size > ::openathan::DUA_ENVELOPE_MAX) return false;
  JsonDocument wrapper;
  if (deserializeJson(wrapper, envelope, size) || !wrapper.is<JsonObject>() || wrapper.size() != 2 ||
      !wrapper["payload"].is<const char *>() || !wrapper["signature"].is<const char *>()) return false;
  const auto payload = wrapper["payload"].as<std::string>(), signature = wrapper["signature"].as<std::string>();
  std::array<uint8_t, 72> sig{}; uint8_t hash[32];
  if (payload.size() > 1024 || signature.size() < 128 || signature.size() > 144 || signature.size() % 2 ||
      !unhex(signature, sig.data(), signature.size() / 2) ||
      mbedtls_sha256(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), hash, 0)) return false;
  mbedtls_pk_context key; mbedtls_pk_init(&key);
  const char *pem = OPENATHAN_UPGRADE_PUBLIC_KEY;
  const bool verified = !mbedtls_pk_parse_public_key(&key, reinterpret_cast<const uint8_t *>(pem), std::strlen(pem) + 1) &&
      !mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, 32, sig.data(), signature.size() / 2);
  mbedtls_pk_free(&key);
  if (!verified) return false;
  JsonDocument doc; std::array<uint8_t, 32> expected{};
  return !deserializeJson(doc, payload) && doc.is<JsonObject>() && doc.size() == 8 &&
      doc["schema"].is<unsigned>() && doc["schema"].as<unsigned>() == 1 &&
      doc["kind"].as<std::string>() == "dua" && doc["mediaId"].as<std::string>() == "post-athan-dua-v1" &&
      doc["layout"].as<std::string>() == "dual-2m-audio-3_5m-v1" &&
      doc["format"].is<unsigned>() && doc["format"].as<unsigned>() == 1 &&
      doc["bytes"].is<uint32_t>() && doc["bytes"].as<uint32_t>() == bytes && bytes && bytes <= ::openathan::DUA_MAX_BYTES &&
      release_version(doc["version"].as<std::string>()).has_value() &&
      (!exact_release || doc["version"].as<std::string>() == OPENATHAN_FIRMWARE_VERSION) &&
      unhex(doc["sha256"].as<std::string>(), expected.data(), 32) && std::memcmp(expected.data(), digest, 32) == 0;
}
bool DuaMedia::scan_() {
  installed_.reset();
  for (unsigned i = 0; i < 2; ++i) {
    const auto *bank = data_ + ::openathan::DUA_BANK_START + i * ::openathan::DUA_BANK_SIZE;
    if (staging_ != i + 1 && !::openathan::dua_bank_recognized(bank) && !::openathan::dua_bank_erased(bank)) return false;
    ::openathan::DuaBank result;
    if (::openathan::validate_dua_bank(bank, i, hash_matches,
        [this](const char *e, size_t n, uint32_t bytes, const uint8_t *hash) { return verify_(e, n, bytes, hash); }, result) &&
        (!installed_ || result.generation > installed_->generation)) installed_ = result;
  }
  return true;
}
void DuaMedia::begin(openathan_component::OpenAthan *athan) {
  athan_ = athan;
  if (!athan_->playback() || !athan_->playback()->dua_supported()) return;
  if (nvs_open(openathan_storage::PRAYER, NVS_READWRITE, &journal_) != ESP_OK) return;
  uint32_t journal{}; size_t length = sizeof(journal);
  const auto loaded = nvs_get_blob(journal_, "dua_stage", &journal, &length);
  if (loaded != ESP_ERR_NVS_NOT_FOUND && (loaded != ESP_OK || length != sizeof(journal) ||
      (journal != 0xd0a00001 && journal != 0xd0a00002))) return;
  if (loaded == ESP_OK) staging_ = journal & 3;
  storage_ok_ = true;
  partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "athan_audio");
  if (!partition_ || partition_->size != ::openathan::AUDIO_PARTITION_SIZE) return;
  const void *mapped{};
  if (esp_partition_mmap(partition_, 0, partition_->size, ESP_PARTITION_MMAP_DATA, &mapped, &mapping_) != ESP_OK) return;
  data_ = static_cast<const uint8_t *>(mapped);
  ::openathan::AudioCatalog base;
  if (::openathan::validate_audio_image(data_, partition_->size, hash_matches, base) != ::openathan::AudioError::OK ||
      ::openathan::read_u32(data_ + 8) > ::openathan::DUA_BANK_START || !scan_()) return;
  compatible_ = true;
  state_ = installed_ ? "ready" : "waiting";
}
void DuaMedia::fail_(const char *error, bool transient) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = "failed"; error_ = error; latched_ = !transient;
  retry_at_ = transient ? now_ms() + 300000 : 0;
}
int DuaMedia::retry(std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!compatible_ || !storage_ok_) { error = "Dua storage is unavailable"; return 503; }
  if (active_ || !athan_->dua_preferences().enabled()) { error = "Enable dua and wait for the current request"; return 409; }
  latched_ = false; retry_at_ = 0; error_.clear(); state_ = "waiting"; return 200;
}
void DuaMedia::loop(bool connected, bool permitted) {
  if (!athan_) return;
  const bool enabled = athan_->dua_preferences().enabled();
  safe_ = enabled && connected && permitted && !athan_->playback_busy();
  if (!enabled || !connected || !permitted || athan_->playback_busy()) cancel_ = true;
  if (active_) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (success_pending_) {
    success_pending_ = false;
    // partition reads/mappings are coherent through the IDF flash APIs.
    compatible_ = scan_();
    if (!compatible_ || !installed_) { state_ = "failed"; error_ = "Dua readback failed"; latched_ = true; }
    else { state_ = "ready"; error_.clear(); }
  }
  if (!compatible_ || !storage_ok_) { state_ = "unavailable"; return; }
  if (installed_) {
    if (!athan_->playback()->dua_ready() && !athan_->playback_busy())
      athan_->playback()->set_dua_data(installed_->payload, installed_->bytes);
    state_ = "ready"; return;
  }
  if (!enabled) { state_ = "off"; error_.clear(); latched_ = false; retry_at_ = 0; return; }
  if (latched_ || now_ms() < retry_at_) return;
  if (!safe_) { state_ = "waiting"; return; }
  // Only the explicitly isolated private-trust qualification component may
  // override the production transport. Ordinary isolated builds stay offline.
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
  constexpr bool private_qualification = true;
#else
  constexpr bool private_qualification = false;
#endif
  if ((!private_qualification && (openathan_storage::TEST_MODE || std::strcmp(OPENATHAN_BUILD_COMMIT, "development") == 0)) ||
      !OPENATHAN_UPDATES_ENABLED) {
    state_ = "unavailable"; error_ = "Dua downloads require production firmware"; return;
  }
  target_ = staging_ ? staging_ - 1 : 0;
  const uint32_t journal = 0xd0a00001 + target_;
  if (nvs_set_blob(journal_, "dua_stage", &journal, sizeof(journal)) != ESP_OK || nvs_commit(journal_) != ESP_OK) {
    storage_ok_ = false; state_ = "unavailable"; error_ = "Dua request could not be saved; restart the device"; return;
  }
  staging_ = target_ + 1; cancel_ = false; active_ = true; received_ = total_ = 0;
  state_ = "downloading"; error_.clear();
  if (xTaskCreate(worker_, "oa_dua", 8192, this, 1, nullptr) != pdPASS) {
    active_ = false; state_ = "failed"; error_ = "Not enough memory; retrying later"; retry_at_ = now_ms() + 300000;
  }
}
void DuaMedia::worker_(void *argument) {
  auto *self = static_cast<DuaMedia *>(argument);
  self->run_();
  self->active_ = false;
  vTaskDelete(nullptr);
}
void DuaMedia::run_() {
  using namespace ::openathan;
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
  const auto base = std::string(openathan_upgrade_qualification::RELEASE_BASE) + "download/" + OPENATHAN_FIRMWARE_VERSION + "/";
#else
  const auto base = std::string("https://github.com/OpenAthan-Project/openathan/releases/download/") + OPENATHAN_FIRMWARE_VERSION + "/";
#endif
  std::unique_ptr<uint8_t, decltype(&heap_caps_free)> buffer(
      static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  std::unique_ptr<uint8_t, decltype(&heap_caps_free)> header(
      static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!buffer || !header) { fail_("Not enough memory to install dua", true); return; }
  std::string envelope;
  {
    release_http::Response response;
    if (!response.open(base + "dua.json")) { fail_("Dua recording could not be downloaded", response.status < 400 || response.status >= 500 || response.status == 408 || response.status == 429); return; }
    const auto deadline = now_ms() + 30000;
    while (allowed_() && now_ms() < deadline) {
      const int count = esp_http_client_read(response.client, reinterpret_cast<char *>(buffer.get()), 1024);
      if (count < 0 || envelope.size() + count > DUA_ENVELOPE_MAX) { fail_("Invalid dua descriptor", false); return; }
      if (!count) break;
      envelope.append(reinterpret_cast<char *>(buffer.get()), count);
    }
    if (!allowed_()) return;
    if (!esp_http_client_is_complete_data_received(response.client)) { fail_("Dua download interrupted", true); return; }
  }
  JsonDocument wrapper, descriptor;
  if (deserializeJson(wrapper, envelope) || !wrapper["payload"].is<const char *>() ||
      deserializeJson(descriptor, wrapper["payload"].as<std::string>())) { fail_("Invalid dua descriptor", false); return; }
  const uint32_t bytes = descriptor["bytes"].as<uint32_t>(); std::array<uint8_t, 32> digest{};
  if (!unhex(descriptor["sha256"].as<std::string>(), digest.data(), 32) ||
      !verify_(envelope.data(), envelope.size(), bytes, digest.data(), true)) { fail_("Dua recording could not be verified", false); return; }
  { std::lock_guard<std::mutex> lock(mutex_); total_ = bytes; }
  const size_t offset = DUA_BANK_START + target_ * DUA_BANK_SIZE;
  for (size_t at = 0; at < DUA_BANK_SIZE; at += 4096) {
    if (!allowed_()) return;
    if (esp_partition_erase_range(partition_, offset + at, 4096) != ESP_OK) { fail_("Dua storage erase failed", false); return; }
    vTaskDelay(1);
  }
  {
    release_http::Response response;
    if (!allowed_()) return;
    if (!response.open(base + "dua.mp3")) { fail_("Dua recording could not be downloaded", response.status < 400 || response.status >= 500 || response.status == 408 || response.status == 429); return; }
    uint32_t received = 0; const auto deadline = now_ms() + 120000;
    while (allowed_() && now_ms() < deadline) {
      const int count = esp_http_client_read(response.client, reinterpret_cast<char *>(buffer.get()), 4096);
      if (!allowed_()) return;
      if (count < 0) { fail_("Dua download interrupted", true); return; }
      if (!count) break;
      if (static_cast<uint32_t>(count) > bytes - received) { fail_("Dua recording exceeds its signed size", false); return; }
      if (esp_partition_write(partition_, offset + DUA_HEADER_SIZE + received, buffer.get(), count) != ESP_OK) {
        fail_("Dua storage write failed", false); return;
      }
      received += count;
      { std::lock_guard<std::mutex> lock(mutex_); received_ = received; }
      vTaskDelay(1);
    }
    if (!allowed_()) return;
    if (received != bytes || !esp_http_client_is_complete_data_received(response.client)) { fail_("Dua download interrupted", true); return; }
  }
  mbedtls_sha256_context hash; mbedtls_sha256_init(&hash); int status = mbedtls_sha256_starts(&hash, 0);
  for (size_t at = 0; at < bytes && !status; at += 4096) {
    if (!allowed_()) { mbedtls_sha256_free(&hash); return; }
    const auto count = std::min(size_t(4096), size_t(bytes - at));
    status = esp_partition_read(partition_, offset + DUA_HEADER_SIZE + at, buffer.get(), count);
    if (!status) status = mbedtls_sha256_update(&hash, buffer.get(), count);
    vTaskDelay(1);
  }
  uint8_t actual[32]; if (!status) status = mbedtls_sha256_finish(&hash, actual); mbedtls_sha256_free(&hash);
  if (status || std::memcmp(actual, digest.data(), 32)) { fail_("Dua storage verification failed", false); return; }
  std::memset(header.get(), 0xff, 4096); std::memcpy(header.get(), "OADUA001", 8);
  put(header.get() + 8, 1); put(header.get() + 12, bytes); put(header.get() + 16, envelope.size()); put(header.get() + 20, 1);
  std::memcpy(header.get() + 24, digest.data(), 32); std::memcpy(header.get() + 64, envelope.data(), envelope.size());
  if (mbedtls_sha256(header.get(), 4060, header.get() + 4060, 0)) { fail_("Dua metadata verification failed", false); return; }
  if (!allowed_()) return;
  if (esp_partition_write(partition_, offset, header.get(), 4092) != ESP_OK) { fail_("Dua metadata write failed", false); return; }
  if (!allowed_()) return;
  if (esp_partition_read(partition_, offset, buffer.get(), 4092) != ESP_OK ||
      std::memcmp(buffer.get(), header.get(), 4092)) { fail_("Dua metadata readback failed", false); return; }
  // The commit marker is the final write; a torn marker never activates bytes.
  if (!allowed_()) return;
  put(header.get(), DUA_COMMIT);
  if (esp_partition_write(partition_, offset + 4092, header.get(), 4) != ESP_OK) { fail_("Dua commit failed", false); return; }
  std::lock_guard<std::mutex> lock(mutex_); success_pending_ = true;
}
void DuaMedia::snapshot(JsonObject root) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto &preferences = athan_->dua_preferences();
  root["schema"] = 1; root["revision"] = preferences.saved() ? preferences.saved()->revision : 0;
  root["enabled"] = preferences.enabled(); root["ready"] = athan_->playback()->dua_ready();
  root["supported"] = compatible_ && storage_ok_;
  root["application"] = preferences.writable() ? "applied" : "storage_fault";
  root["state"] = state_; root["error"] = error_; root["received_bytes"] = received_; root["total_bytes"] = total_;
  root["phase"] = ::openathan::dua_phase_name(athan_->dua_sequence().phase());
  root["playback_error"] = athan_->dua_sequence().failed();
}
}  // namespace esphome::openathan_device
