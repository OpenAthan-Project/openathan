#pragma once
#include "esphome/core/component.h"
#include "esphome/components/speaker/media_player/speaker_media_player.h"
#include "openathan/audio_image.h"
#include "openathan/playback.h"
#include <esp_partition.h>

namespace esphome::openathan_audio {
class PartitionAudio : public Component, public ::openathan::Playback {
 public:
  void set_player(speaker::SpeakerMediaPlayer *player) { player_ = player; }
  void set_partition(const std::string &partition) { partition_ = partition; }
  void setup() override;
  void loop() override;
  bool play(::openathan::Track track);
  ::openathan::PlaybackCompletion completion() const override { return completion_; }
  bool dua_supported() const override {
#if defined(OPENATHAN_QURAN) || defined(OPENATHAN_DUA_PIPELINE)
    return true;
#else
    return false;
#endif
  }
  bool dua_ready() const override { return ready() && dua_file_.data && dua_file_.length; }
  bool set_dua_data(const uint8_t *data, size_t size) override {
#if defined(OPENATHAN_QURAN) || defined(OPENATHAN_DUA_PIPELINE)
    if (source_ != ::openathan::PlaybackSource::NONE || playing() || (player_ && !player_->playback_stopped())) return false;
    dua_file_ = {data, size, audio::AudioFileType::MP3}; return true;
#else
    return false;
#endif
  }
  bool ready() const override { return ready_ && player_ && player_->is_ready(); }
  bool playing() const override;
  bool start(::openathan::Track track) override;
  void stop() override;
  bool stream_supported() const override {
#ifdef OPENATHAN_QURAN
    return true;
#else
    return false;
#endif
  }
  uint32_t playback_epoch() const override { return epoch_; }
  ::openathan::PlaybackSource source() const override { return source_; }
  ::openathan::StreamState stream_state() const override { return stream_state_; }
  bool start_stream(const std::string &url) override;
  std::optional<unsigned> volume_percent() const override;
  bool request_volume_percent(unsigned percent) override;
  // Hardware adapters may split attenuation between the codec and software.
  std::optional<float> volume_level() const;
  bool request_volume_level(float level);
 protected:
  speaker::SpeakerMediaPlayer *player_{};
  std::string partition_;
  // Both the map and AudioFile objects must outlive asynchronous player queues.
  esp_partition_mmap_handle_t mapping_{};
  std::array<audio::AudioFile, 2> files_{};
  bool ready_{false};
  uint32_t epoch_{};
  ::openathan::PlaybackSource source_{::openathan::PlaybackSource::NONE};
  ::openathan::StreamState stream_state_{::openathan::StreamState::IDLE};
  bool observed_playing_{};
  ::openathan::PlaybackCompletion completion_;
  audio::AudioFile dua_file_{};
  uint32_t submitted_session_{};
  uint32_t start_ms_{};
  // Reserve ownership while the outgoing commands and pipelines drain. Only
  // one replacement survives; Stop clears its recording and owned URL.
  bool awaiting_stop_{};
  audio::AudioFile *pending_file_{};
  std::string pending_url_;
  uint32_t stream_started_{};
};
}  // namespace esphome::openathan_audio
