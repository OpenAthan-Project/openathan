#include "partition_audio.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <mbedtls/sha256.h>
#include <algorithm>

namespace esphome::openathan_audio {
static const char *const TAG = "openathan_audio";

static bool hash_matches(const uint8_t *data, size_t size, const uint8_t *expected) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  int status = mbedtls_sha256_starts(&ctx, 0);
  while (size && status == 0) {
    const size_t chunk = std::min(size, size_t(8192));
    status = mbedtls_sha256_update(&ctx, data, chunk);
    data += chunk;
    size -= chunk;
    App.feed_wdt();
  }
  uint8_t digest[32];
  if (status == 0)
    status = mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  return status == 0 && std::memcmp(digest, expected, sizeof(digest)) == 0;
}

void PartitionAudio::setup() {
  const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                                   partition_.c_str());
  if (!partition || partition->size != ::openathan::AUDIO_PARTITION_SIZE) {
    ESP_LOGE(TAG, "Missing or incorrectly sized shared audio partition");
    this->mark_failed();
    return;
  }
  const void *mapped = nullptr;
  const esp_err_t status = esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA,
                                             &mapped, &mapping_);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Audio partition mapping failed: %s", esp_err_to_name(status));
    this->mark_failed();
    return;
  }
  ::openathan::AudioCatalog catalog;
  const auto *data = static_cast<const uint8_t *>(mapped);
  const auto error = ::openathan::validate_audio_image(data, partition->size, hash_matches, catalog);
  if (error != ::openathan::AudioError::OK) {
    ESP_LOGE(TAG, "Invalid audio image, error=%u; playback disabled", unsigned(error));
    esp_partition_munmap(mapping_);
    this->mark_failed();
    return;
  }
  for (size_t i = 0; i < files_.size(); ++i)
    files_[i] = {data + catalog[i].offset, catalog[i].length, audio::AudioFileType::MP3};
  ready_ = true;
  ESP_LOGI(TAG, "Both shared MP3 tracks verified; playback available");
  // Keep the mapping until reboot. No audio writes occur in this component.
}

bool PartitionAudio::play(::openathan::Track track) {
  const auto id = static_cast<unsigned>(track);
  if (!ready() || id < 1 || id > files_.size()) {
    ESP_LOGW(TAG, "Playback unavailable or invalid track");
    return false;
  }
  player_->play_file(&files_[id - 1], true, false);
  ESP_LOGI(TAG, "Playback queued: track=%u", id);
  return true;
}

bool PartitionAudio::playing() const {
  return player_ && (player_->state == media_player::MEDIA_PLAYER_STATE_PLAYING ||
                     player_->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
}
void PartitionAudio::stop() {
  if (!player_ || !player_->is_ready()) return;
  // Stop both pipelines. Commands and the subsequent file request share a FIFO.
  for (bool announcement : {false, true}) {
    auto call = player_->make_call();
    call.set_command(media_player::MEDIA_PLAYER_COMMAND_STOP);
    call.set_announcement(announcement);
    call.perform();
  }
}
bool PartitionAudio::start(::openathan::Track track) {
  if (!ready() || (track != ::openathan::Track::NORMAL && track != ::openathan::Track::FAJR)) return false;
  stop();
  return play(track);
}
}  // namespace esphome::openathan_audio
