#include "athan.h"
#include "upgrade.h"
#include "release_http.h"
#include "esphome/core/application.h"
#include "openathan/dua_media.h"
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <algorithm>
#include <cstring>

namespace esphome::openathan_device {
namespace {
constexpr uint32_t MEDIA_END = 0x300000;
static_assert(MEDIA_END == ::openathan::DUA_BANK_START);
constexpr size_t ENVELOPE_MAX = 32768, PAYLOAD_MAX = 24576;
constexpr const char *BASE = "https://github.com/OpenAthan-Project/athan-audio/releases/";
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
uint32_t fajr_offset(uint32_t normal) { return (4096 + normal + 15) & ~uint32_t(15); }
void put(uint8_t *at, uint32_t value) { for (unsigned i = 0; i < 4; ++i) at[i] = value >> (8 * i); }
bool unhex(const std::string &text, uint8_t *out, size_t size) {
  if (text.size() != size * 2) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (size_t i = 0; i < size; ++i) {
    const int a = digit(text[i * 2]), b = digit(text[i * 2 + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = a * 16 + b;
  }
  return true;
}
std::string hex(const uint8_t *data, size_t size) {
  std::string out; out.reserve(size * 2);
  for (size_t i = 0; i < size; ++i) { out += "0123456789abcdef"[data[i] >> 4]; out += "0123456789abcdef"[data[i] & 15]; }
  return out;
}
bool text(JsonVariantConst field, std::string &out, size_t maximum) {
  if (!field.is<const char *>()) return false;
  const auto value = field.as<JsonString>();
  if (!value.size() || value.size() > maximum || std::memchr(value.c_str(), 0, value.size())) return false;
  out.assign(value.c_str(), value.size());
  return true;
}
bool id_valid(const std::string &id) {
  return !id.empty() && id.size() <= 48 && std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
  });
}
bool file_valid(const std::string &file) {
  return !file.empty() && file.size() <= 80 && file[0] != '.' && file.find("..") == std::string::npos &&
      std::all_of(file.begin(), file.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
      });
}
bool quoted(std::string &out, std::string_view value) {
  out += '"';
  const auto unit = [&](uint32_t code) {
    out += "\\u";
    for (int shift = 12; shift >= 0; shift -= 4) out += "0123456789abcdef"[(code >> shift) & 15];
  };
  for (size_t i = 0; i < value.size();) {
    const uint8_t first = value[i++];
    if (first < 0x20 || first == 0x7f) return false;
    if (first < 0x80) {
      if (first == '"' || first == '\\') out += '\\';
      out += char(first); continue;
    }
    unsigned trailing; uint32_t code, minimum;
    if (first >= 0xc2 && first <= 0xdf) { trailing = 1; code = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { trailing = 2; code = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { trailing = 3; code = first & 7; minimum = 0x10000; }
    else return false;
    if (trailing > value.size() - i) return false;
    while (trailing--) {
      const uint8_t next = value[i++];
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 63);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    if (code <= 0xffff) unit(code);
    else { code -= 0x10000; unit(0xd800 + (code >> 10)); unit(0xdc00 + (code & 1023)); }
  }
  out += '"'; return true;
}
template<size_t N> void copy(char (&out)[N], const std::string &value) {
  std::memset(out, 0, N); std::memcpy(out, value.data(), std::min(N - 1, value.size()));
}
bool hash(const uint8_t *data, size_t size, const uint8_t *expected) {
  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx);
  int result = mbedtls_sha256_starts(&ctx, 0);
  while (size && !result) {
    const size_t count = std::min(size, size_t(4096));
    result = mbedtls_sha256_update(&ctx, data, count); data += count; size -= count; App.feed_wdt();
  }
  uint8_t actual[32];
  if (!result) result = mbedtls_sha256_finish(&ctx, actual);
  mbedtls_sha256_free(&ctx);
  return !result && !std::memcmp(actual, expected, sizeof(actual));
}
bool boot_safe() {
  const auto *running = esp_ota_get_running_partition(), *boot = esp_ota_get_boot_partition();
  if (!running || !boot || running->address != boot->address || running->type != ESP_PARTITION_TYPE_APP) return false;
  esp_ota_img_states_t state{};
  const auto result = esp_ota_get_state_partition(running, &state);
  if (result == ESP_OK) return state == ESP_OTA_IMG_VALID || state == ESP_OTA_IMG_UNDEFINED;
  if (result != ESP_ERR_NOT_FOUND) return false;
  const auto *data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!data || data->size != 8192) return false;
  std::array<uint8_t, 128> bytes{};
  for (size_t at = 0; at < data->size; at += bytes.size()) {
    if (esp_partition_read(data, at, bytes.data(), bytes.size()) != ESP_OK ||
        !std::all_of(bytes.begin(), bytes.end(), [](uint8_t byte) { return byte == 0xff; })) return false;
  }
  return true;
}
template<typename Pair> bool same_pair(const Pair &a, const Pair &b) {
  for (size_t i = 0; i < 2; ++i) if (!a[i].bytes || a[i].bytes != b[i].bytes || a[i].sha256 != b[i].sha256) return false;
  return true;
}
}

bool AthanMedia::save_(Record next) {
  if (!storage_ok_) return false;
  if (mbedtls_sha256(reinterpret_cast<const uint8_t *>(&next), offsetof(Record, checksum), next.checksum.data(), 0)) return false;
  Record observed{}; size_t bytes = sizeof(observed);
  if (nvs_set_blob(nvs_, "record", &next, sizeof(next)) != ESP_OK || nvs_commit(nvs_) != ESP_OK ||
      nvs_get_blob(nvs_, "record", &observed, &bytes) != ESP_OK || bytes != sizeof(next) || std::memcmp(&next, &observed, sizeof(next))) {
    storage_ok_ = false; error_ = "Recording request could not be confirmed; restart the speaker"; return false;
  }
  record_ = next; return true;
}
bool AthanMedia::load_() {
  size_t bytes = sizeof(record_);
  const auto result = nvs_get_blob(nvs_, "record", &record_, &bytes);
  if (result == ESP_ERR_NVS_NOT_FOUND) { record_ = {}; return true; }
  if (result != ESP_OK || bytes != sizeof(record_) || std::memcmp(record_.magic, "OAATH001", 8) ||
      record_.phase > Phase::REBOOT || record_.reserved ||
      !hash(reinterpret_cast<const uint8_t *>(&record_), offsetof(Record, checksum), record_.checksum.data())) return false;
  for (const auto *pair : {&record_.installed, &record_.requested}) {
    for (const auto &track : *pair) {
      if (!std::memchr(track.id, 0, sizeof(track.id)) || !std::memchr(track.name, 0, sizeof(track.name)) || track.reserved ||
          track.bytes > MEDIA_END - 4096 || (track.id[0] && !id_valid(track.id))) return false;
    }
  }
  if (record_.phase != Phase::IDLE && (!std::memchr(record_.version, 0, sizeof(record_.version)) ||
      !release_version(record_.version) || !record_.requested[0].id[0] || !record_.requested[1].id[0] ||
      !record_.requested[0].bytes || !record_.requested[1].bytes ||
      fajr_offset(record_.requested[0].bytes) + record_.requested[1].bytes > MEDIA_END)) return false;
  return true;
}
bool AthanMedia::scan_(std::array<SavedTrack, 2> &tracks) {
  Buffer buffer(static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!buffer || esp_partition_read(partition_, 0, buffer.get(), 4096) != ESP_OK ||
      std::memcmp(buffer.get(), "OAUDIO01", 8) || ::openathan::read_u32(buffer.get() + 12) != 2 ||
      !hash(buffer.get(), 112, buffer.get() + 112)) return false;
  const uint32_t used = ::openathan::read_u32(buffer.get() + 8);
  if (used < 4096 || used > MEDIA_END) return false;
  std::array<uint32_t, 2> offsets{};
  uint32_t end = 4096;
  for (size_t i = 0; i < 2; ++i) {
    const auto *entry = buffer.get() + 16 + i * 48;
    const auto offset = ::openathan::read_u32(entry + 4), length = ::openathan::read_u32(entry + 8);
    if (::openathan::read_u32(entry) != i + 1 || ::openathan::read_u32(entry + 12) != 1 ||
        offset % 16 || offset < end || offset > used || !length || length > used - offset) return false;
    tracks[i] = {}; tracks[i].bytes = length; offsets[i] = offset;
    std::copy_n(entry + 16, 32, tracks[i].sha256.begin()); end = offset + length;
  }
  if (end != used) return false;
  for (size_t i = 0; i < 2; ++i) {
    mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); int result = mbedtls_sha256_starts(&ctx, 0);
    for (uint32_t at = 0; !result && at < tracks[i].bytes; at += 4096) {
      const auto count = std::min(uint32_t(4096), tracks[i].bytes - at);
      result = esp_partition_read(partition_, offsets[i] + at, buffer.get(), count);
      if (!result) result = mbedtls_sha256_update(&ctx, buffer.get(), count);
      App.feed_wdt();
    }
    uint8_t digest[32]; if (!result) result = mbedtls_sha256_finish(&ctx, digest); mbedtls_sha256_free(&ctx);
    if (result || std::memcmp(digest, tracks[i].sha256.data(), 32)) return false;
  }
  return true;
}
void AthanMedia::begin(openathan_component::OpenAthan *athan) {
  athan_ = athan;
  started_ms_ = now_ms();
  supported_ = !openathan_storage::TEST_MODE && OPENATHAN_UPDATES_ENABLED && std::strcmp(OPENATHAN_BUILD_COMMIT, "development") &&
      athan_ && athan_->playback() && athan_->playback()->media_storage_supported();
  if (!supported_) return;
  partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "athan_audio");
  if (!partition_ || partition_->size != ::openathan::AUDIO_PARTITION_SIZE || partition_->erase_size != 4096) return;
  const auto *running = esp_ota_get_running_partition();
  if (!running || esp_partition_get_sha256(running, firmware_.data()) != ESP_OK) return;
  if (nvs_open("oa_athan", NVS_READWRITE, &nvs_) != ESP_OK || !load_()) {
    error_ = "Recording request storage is unavailable; restart the speaker"; return;
  }
  storage_ok_ = true;
  identity_ok_ = record_.phase <= Phase::QUEUED || record_.firmware == firmware_;
  std::array<SavedTrack, 2> physical{};
  const bool valid = scan_(physical);
  installed_ = record_.installed;
  if (valid) {
    for (size_t i = 0; i < 2; ++i) {
      if (physical[i].bytes != installed_[i].bytes || physical[i].sha256 != installed_[i].sha256) installed_[i] = physical[i];
    }
  }
  destructive_ = record_.phase == Phase::WRITING || record_.phase == Phase::REBOOT;
  boot_verified_ = destructive_ && valid && same_pair(physical, record_.requested);
  if (destructive_) {
    // Intent remains distinct from installed identity until startup is healthy.
    installed_ = record_.installed;
    state_ = boot_verified_ ? "restarting" : "repair_needed";
    if (!boot_verified_) quiesce_();
  } else if (!valid) { state_ = "unavailable"; error_ = "Installed audio is invalid without a repair receipt"; }
  else state_ = record_.phase == Phase::QUEUED ? "waiting" : installed_[0].id[0] && installed_[1].id[0] ? "ready" : "idle";
  if (!identity_ok_) { latched_ = true; state_ = "repair_needed"; error_ = "Repair requires the firmware that started this replacement"; }
}
bool AthanMedia::verify_catalog_(const std::string &envelope, Catalog &out) {
  if (envelope.empty() || envelope.size() > ENVELOPE_MAX) return false;
  JsonDocument wrapper;
  if (deserializeJson(wrapper, envelope) || !wrapper.is<JsonObject>() || wrapper.size() != 2) return false;
  std::string payload, signature;
  if (!text(wrapper["payload"], payload, PAYLOAD_MAX) || !text(wrapper["signature"], signature, 144) || signature.size() < 128 || signature.size() % 2) return false;
  std::array<uint8_t, 72> sig{};
  if (!unhex(signature, sig.data(), signature.size() / 2) ||
      mbedtls_sha256(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), out.hash.data(), 0)) return false;
  mbedtls_pk_context key; mbedtls_pk_init(&key);
  const char *pem = OPENATHAN_UPGRADE_PUBLIC_KEY;
  const bool verified = !mbedtls_pk_parse_public_key(&key, reinterpret_cast<const uint8_t *>(pem), std::strlen(pem) + 1) &&
      !mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, out.hash.data(), out.hash.size(), sig.data(), signature.size() / 2);
  mbedtls_pk_free(&key);
  if (!verified) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>() || doc.size() != 4 ||
      !doc["schema"].is<unsigned>() || doc["schema"].as<unsigned>() != 1 ||
      doc["kind"].as<std::string>() != "athan_catalog" || !text(doc["version"], out.version, 33) || !release_version(out.version) ||
      !doc["recordings"].is<JsonArray>() || doc["recordings"].size() > 16) return false;
  uint32_t largest[2]{};
  for (JsonObjectConst item : doc["recordings"].as<JsonArrayConst>()) {
    Recording recording; std::string id, name, digest, role;
    if (item.size() != 9 || !text(item["id"], id, 48) || !id_valid(id) || !text(item["name"], name, 80) ||
        !text(item["role"], role, 6) || (role != "normal" && role != "fajr") || !item["bytes"].is<uint32_t>() ||
        !text(item["sha256"], digest, 64) || !unhex(digest, recording.saved.sha256.data(), 32) ||
        !text(item["file"], recording.file, 80) || !file_valid(recording.file) ||
        !text(item["attribution"], recording.attribution, PAYLOAD_MAX) || !text(item["source"], recording.source, PAYLOAD_MAX) ||
        !text(item["license"], recording.license, PAYLOAD_MAX)) return false;
    if (std::any_of(out.recordings.begin(), out.recordings.end(), [&](const auto &other) { return other.saved.id == id; })) return false;
    recording.saved.bytes = item["bytes"].as<uint32_t>();
    if (!recording.saved.bytes || recording.saved.bytes > MEDIA_END - 4096) return false;
    copy(recording.saved.id, id); copy(recording.saved.name, name); recording.role = role == "fajr";
    largest[recording.role] = std::max(largest[recording.role], recording.saved.bytes);
    out.recordings.push_back(std::move(recording));
  }
  if (!largest[0] || !largest[1] || fajr_offset(largest[0]) + largest[1] > MEDIA_END) return false;
  std::string canonical = "{\"kind\":\"athan_catalog\",\"recordings\":[";
  for (size_t i = 0; i < out.recordings.size(); ++i) {
    const auto &recording = out.recordings[i];
    if (i) canonical += ',';
    canonical += "{\"attribution\":";
    if (!quoted(canonical, recording.attribution)) return false;
    canonical += ",\"bytes\":" + std::to_string(recording.saved.bytes) + ",\"file\":";
    if (!quoted(canonical, recording.file)) return false;
    canonical += ",\"id\":"; if (!quoted(canonical, recording.saved.id)) return false;
    canonical += ",\"license\":"; if (!quoted(canonical, recording.license)) return false;
    canonical += ",\"name\":"; if (!quoted(canonical, recording.saved.name)) return false;
    canonical += ",\"role\":"; quoted(canonical, recording.role ? "fajr" : "normal");
    canonical += ",\"sha256\":"; quoted(canonical, hex(recording.saved.sha256.data(), 32));
    canonical += ",\"source\":"; if (!quoted(canonical, recording.source)) return false;
    canonical += '}';
  }
  canonical += "],\"schema\":1,\"version\":"; quoted(canonical, out.version); canonical += '}';
  return canonical == payload;
}
bool AthanMedia::fetch_catalog_(const std::string &url, Catalog &out) {
  Buffer buffer(static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!buffer) return false;
  release_http::Response response;
  if (!network_allowed_() || !response.open(url)) return false;
  std::string envelope;
  const auto deadline = now_ms() + 30000;
  while (network_allowed_() && now_ms() < deadline) {
    const int count = esp_http_client_read(response.client, reinterpret_cast<char *>(buffer.get()), 4096);
    if (count < 0 || envelope.size() + count > ENVELOPE_MAX) return false;
    if (!count) break;
    envelope.append(reinterpret_cast<char *>(buffer.get()), count);
  }
  return network_allowed_() && esp_http_client_is_complete_data_received(response.client) && verify_catalog_(envelope, out);
}
bool AthanMedia::download_(const std::string &url, const SavedTrack &track, uint8_t *out) {
  Buffer buffer(static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!buffer) return false;
  release_http::Response response;
  if (!network_allowed_() || !response.open(url)) return false;
  uint32_t received = 0; const auto deadline = now_ms() + 180000;
  while (network_allowed_() && now_ms() < deadline) {
    const int count = esp_http_client_read(response.client, reinterpret_cast<char *>(buffer.get()), 4096);
    if (count < 0 || uint32_t(count) > track.bytes - received) return false;
    if (!count) break;
    std::memcpy(out + received, buffer.get(), count); received += count;
    { std::lock_guard<std::mutex> lock(mutex_); received_ += count; }
    vTaskDelay(1);
  }
  return network_allowed_() && received == track.bytes && esp_http_client_is_complete_data_received(response.client) && hash(out, track.bytes, track.sha256.data());
}
bool AthanMedia::start_(Job job) {
  job_ = job; cancel_ = false; active_ = true;
  if (xTaskCreate(worker_, "oa_athan", 8192, this, 1, nullptr) == pdPASS) return true;
  active_ = false; latched_ = true;
  if (job == Job::CATALOG) { refresh_ = false; catalog_state_ = "failed"; catalog_error_ = "Not enough memory to load the catalog"; }
  else { state_ = destructive_ ? "repair_needed" : "failed"; error_ = "Not enough memory; retry the recording request"; }
  return false;
}
void AthanMedia::fail_(const char *message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!storage_ok_ || (cancel_ && !destructive_)) return;
  latched_ = true; state_ = destructive_ ? "repair_needed" : "failed"; error_ = message;
}
void AthanMedia::quiesce_() {
  if (quiesced_) return;
  quiesced_ = true;
  athan_->quiesce_for_maintenance();
  athan_->playback()->lock_media_storage();
}
void AthanMedia::loop(bool connected, bool permitted, bool firmware_idle, bool firmware_confirmed, bool dua_ready) {
  network_safe_ = connected && permitted && firmware_idle && (destructive_ || !athan_->playback_busy());
  if (active_) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!supported_ || !storage_ok_ || !identity_ok_ || state_ == "unavailable") return;
  if (boot_verified_) {
    if (!firmware_idle || !boot_safe() || !athan_->upgrade_health() || !dua_ready) {
      if (now_ms() - started_ms_ >= 90000) {
        state_ = "failed"; error_ = "Recordings verified, but playback startup is incomplete; restart the speaker";
      }
      return;
    }
    Record next = record_; next.installed = record_.requested; next.requested = {}; next.phase = Phase::IDLE;
    if (!save_(next)) return;
    installed_ = next.installed; destructive_ = false; boot_verified_ = false; state_ = "ready"; error_.clear();
    return;
  }
  if (write_finished_) {
    if (!reboot_requested_) { reboot_requested_ = true; state_ = "restarting"; App.safe_reboot(); }
    return;
  }
  if (record_.phase == Phase::IDLE) {
    staged_.reset(); staged_ready_ = false;
    if (refresh_ && network_safe_) start_(Job::CATALOG);
    return;
  }
  if (latched_) return;
  if (!boot_safe() || !firmware_idle || !permitted) { state_ = destructive_ ? "repair_needed" : "waiting"; return; }
  const auto clock = athan_->read(); const auto status = athan_->status();
  const bool window = !athan_->playback_busy() && firmware_confirmed && athan_->activated() &&
      upgrade_window(false, clock.valid, true, status.automatic_ready, clock.utc,
          status.next ? std::optional<int64_t>(status.next->utc) : std::nullopt);
  if (!destructive_ && !window) { state_ = "waiting"; return; }
  if (staged_ready_) {
    if (!destructive_) {
      if (!athan_->playback()->media_storage_idle()) { state_ = "waiting"; return; }
      Record next = record_; next.phase = Phase::WRITING; next.firmware = firmware_;
      if (!save_(next)) { destructive_ = true; return; }
      destructive_ = true;
    }
    quiesce_();
    if (!athan_->playback()->media_storage_idle()) return;
    state_ = "installing"; start_(Job::WRITE); return;
  }
  if (!network_safe_) { state_ = destructive_ ? "repair_needed" : "waiting"; return; }
  state_ = "downloading"; received_ = 0; total_ = record_.requested[0].bytes + record_.requested[1].bytes;
  start_(Job::DOWNLOAD);
}
void AthanMedia::worker_(void *argument) {
  auto *self = static_cast<AthanMedia *>(argument); self->run_(); self->active_ = false; vTaskDelete(nullptr);
}
void AthanMedia::run_() {
  if (job_ == Job::WRITE) { if (!write_()) fail_("Recording storage failed; connect to repair the selected pair"); return; }
  Catalog fetched;
  Record request;
  { std::lock_guard<std::mutex> lock(mutex_); request = record_; }
  const std::string base = std::string(BASE) + "download/" + request.version + "/";
  if (!fetch_catalog_(job_ == Job::CATALOG ? std::string(BASE) + "latest/download/catalog.json" : base + "catalog.json", fetched)) {
    if (job_ == Job::CATALOG) {
      std::lock_guard<std::mutex> lock(mutex_); refresh_ = false; catalog_state_ = "failed"; catalog_error_ = "Catalog download or signature verification failed";
    } else fail_("Recording catalog could not be downloaded and verified");
    return;
  }
  if (job_ == Job::CATALOG) {
    std::lock_guard<std::mutex> lock(mutex_); catalog_ = std::move(fetched); refresh_ = false; catalog_state_ = "ready"; catalog_error_.clear(); return;
  }
  if (fetched.version != request.version || fetched.hash != request.catalog) { fail_("The catalog does not match the saved recording request"); return; }
  std::array<const Recording *, 2> choices{};
  for (const auto &recording : fetched.recordings) {
    const auto &expected = request.requested[recording.role];
    if (std::strcmp(recording.saved.id, expected.id) == 0 && recording.saved.bytes == expected.bytes && recording.saved.sha256 == expected.sha256)
      choices[recording.role] = &recording;
  }
  if (!choices[0] || !choices[1]) { fail_("The saved recording choices are not in this catalog"); return; }
  Buffer pair(static_cast<uint8_t *>(heap_caps_malloc(request.requested[0].bytes + request.requested[1].bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!pair) { fail_("Not enough memory to download both recordings"); return; }
  for (size_t i = 0; i < 2; ++i) {
    if (!download_(base + choices[i]->file, request.requested[i], pair.get() + (i ? request.requested[0].bytes : 0))) {
      fail_("Recording download, size, or checksum verification failed"); return;
    }
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (cancel_ || record_.phase == Phase::IDLE) return;
  staged_ = std::move(pair); staged_ready_ = true; catalog_ = std::move(fetched); catalog_state_ = "ready"; catalog_error_.clear();
}
bool AthanMedia::write_() {
  Buffer buffer(static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  Buffer header(static_cast<uint8_t *>(heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), &heap_caps_free);
  if (!buffer || !header || !staged_ || (record_.phase != Phase::WRITING && record_.phase != Phase::REBOOT)) return false;
  const uint32_t offsets[2]{4096, fajr_offset(record_.requested[0].bytes)};
  const uint32_t used = offsets[1] + record_.requested[1].bytes;
  if (used > MEDIA_END) return false;
  std::memset(header.get(), 0xff, 4096); std::memcpy(header.get(), "OAUDIO01", 8);
  put(header.get() + 8, used); put(header.get() + 12, 2);
  for (size_t i = 0; i < 2; ++i) {
    auto *entry = header.get() + 16 + i * 48;
    put(entry, i + 1); put(entry + 4, offsets[i]); put(entry + 8, record_.requested[i].bytes); put(entry + 12, 1);
    std::copy(record_.requested[i].sha256.begin(), record_.requested[i].sha256.end(), entry + 16);
  }
  if (mbedtls_sha256(header.get(), 112, header.get() + 112, 0)) return false;
  for (uint32_t at = 0; at < used; at += 4096) {
    if (at > MEDIA_END - 4096 || esp_partition_erase_range(partition_, at, 4096) != ESP_OK) return false;
    vTaskDelay(1);
  }
  for (size_t i = 0; i < 2; ++i) {
    const auto *data = staged_.get() + (i ? record_.requested[0].bytes : 0);
    for (uint32_t at = 0; at < record_.requested[i].bytes; at += 4096) {
      const auto count = std::min(uint32_t(4096), record_.requested[i].bytes - at);
      std::memcpy(buffer.get(), data + at, count);
      if (esp_partition_write(partition_, offsets[i] + at, buffer.get(), count) != ESP_OK ||
          esp_partition_read(partition_, offsets[i] + at, buffer.get(), count) != ESP_OK || std::memcmp(buffer.get(), data + at, count)) return false;
      vTaskDelay(1);
    }
  }
  if (esp_partition_write(partition_, 0, header.get(), 4096) != ESP_OK) return false;
  std::array<SavedTrack, 2> actual{};
  if (!scan_(actual) || !same_pair(actual, record_.requested)) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  Record next = record_; next.phase = Phase::REBOOT;
  if (!save_(next)) return false;
  staged_.reset(); staged_ready_ = false; write_finished_ = true; state_ = "restarting"; error_.clear(); return true;
}
bool AthanMedia::can_retry_() const {
  return supported_ && storage_ok_ && identity_ok_ && !active_ && !boot_verified_ &&
      record_.phase != Phase::IDLE && !write_finished_ && (state_ == "failed" || (state_ == "repair_needed" && latched_));
}
int AthanMedia::action(const std::string &action, JsonObjectConst input, std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!supported_ || !storage_ok_ || !identity_ok_ || state_ == "unavailable") { error = "Recording storage is unavailable"; return 503; }
  if (action != "selection" && input.size()) { error = "Invalid recording request"; return 400; }
  if (action == "catalog") {
    if (active_ || record_.phase != Phase::IDLE) { error = "Wait for the recording request to finish"; return 409; }
    refresh_ = true; catalog_state_ = "loading"; catalog_error_.clear(); return 202;
  }
  if (action == "retry") {
    if (!can_retry_()) { error = "There is no recording request to retry"; return 409; }
    latched_ = false; error_.clear(); state_ = destructive_ ? "repair_needed" : "waiting"; return 202;
  }
  if (action == "cancel") {
    if (record_.phase != Phase::QUEUED) { error = "This recording request cannot be cancelled"; return 409; }
    Record next = record_; next.phase = Phase::IDLE; next.requested = {};
    if (next.revision == UINT32_MAX) { error = "Recording revision is exhausted"; return 503; }
    ++next.revision;
    if (!save_(next)) { error = error_; return 503; }
    cancel_ = true; latched_ = false; state_ = "idle"; error_.clear(); return 202;
  }
  if (action != "selection") { error = "Unknown recording action"; return 404; }
  std::string normal, fajr;
  if (input.size() != 4 || !input["schema"].is<unsigned>() || input["schema"].as<unsigned>() != 1 ||
      !input["expected_revision"].is<uint32_t>() || !text(input["normal"], normal, 48) || !text(input["fajr"], fajr, 48)) {
    error = "Invalid recording selection"; return 400;
  }
  if (input["expected_revision"].as<uint32_t>() != record_.revision) { error = "Reload recordings before selecting"; return 409; }
  if (active_ || record_.phase != Phase::IDLE || catalog_state_ != "ready" || !athan_->activated()) {
    error = "Wait for the current request and load the catalog"; return 409;
  }
  Record next = record_; next.requested = {};
  for (const auto &recording : catalog_.recordings) {
    if (recording.saved.id == (recording.role ? fajr : normal)) next.requested[recording.role] = recording.saved;
  }
  if (!next.requested[0].bytes || !next.requested[1].bytes) { error = "Choose one regular and one Fajr recording from this catalog"; return 400; }
  if (next.revision == UINT32_MAX) { error = "Recording revision is exhausted"; return 503; }
  ++next.revision; next.installed = installed_; next.firmware = firmware_; next.catalog = catalog_.hash;
  copy(next.version, catalog_.version);
  const bool unchanged = same_pair(installed_, next.requested);
  if (unchanged) { next.installed = next.requested; next.requested = {}; next.phase = Phase::IDLE; }
  else next.phase = Phase::QUEUED;
  if (!save_(next)) { error = error_; return 503; }
  installed_ = next.installed; state_ = unchanged ? "ready" : "waiting"; error_.clear(); latched_ = false;
  return 202;
}
void AthanMedia::snapshot(JsonObject root) {
  std::lock_guard<std::mutex> lock(mutex_);
  root["schema"] = 1; root["supported"] = supported_;
  root["revision"] = record_.revision; root["state"] = storage_ok_ ? state_ : "unavailable"; root["error"] = error_;
  root["received_bytes"] = received_; root["total_bytes"] = total_;
  root["can_cancel"] = storage_ok_ && record_.phase == Phase::QUEUED; root["can_retry"] = can_retry_();
  auto installed = root["installed"].to<JsonObject>();
  for (size_t i = 0; i < 2; ++i) {
    auto item = installed[i ? "fajr" : "normal"].to<JsonObject>();
    item["id"] = installed_[i].id;
    item["name"] = installed_[i].name[0] ? installed_[i].name : i ? "Installed Fajr Athan" : "Installed regular Athan";
    item["bytes"] = installed_[i].bytes; item["sha256"] = installed_[i].bytes ? hex(installed_[i].sha256.data(), 32) : "";
  }
  if (record_.phase == Phase::IDLE) root["requested"] = nullptr;
  else { root["requested"]["normal"] = record_.requested[0].id; root["requested"]["fajr"] = record_.requested[1].id; }
  auto catalog = root["catalog"].to<JsonObject>();
  catalog["state"] = catalog_state_; catalog["version"] = catalog_.version; catalog["error"] = catalog_error_;
  auto recordings = catalog["recordings"].to<JsonArray>();
  for (const auto &recording : catalog_.recordings) {
    auto item = recordings.add<JsonObject>(); item["id"] = recording.saved.id; item["role"] = recording.role ? "fajr" : "normal";
    item["name"] = recording.saved.name; item["bytes"] = recording.saved.bytes; item["sha256"] = hex(recording.saved.sha256.data(), 32);
    item["attribution"] = recording.attribution; item["source"] = recording.source; item["license"] = recording.license;
  }
}
}
