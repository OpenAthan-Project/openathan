#pragma once
#include "esphome/components/voice_pyramid/voice_pyramid.h"
#include "volume_store.h"
#include "esphome/components/es8311/es8311.h"
#include "esphome/components/openathan_audio/partition_audio.h"
#include "esphome/components/i2s_audio/speaker/i2s_audio_speaker.h"
#include <esp_timer.h>

namespace esphome::voice_pyramid_audio {
class Audio : public Component, public ::openathan::Playback {
 public:
  explicit Audio(std::string storage_namespace = openathan_storage::PRAYER) : settings_store_(std::move(storage_namespace)) {}
  void set_playback(openathan_audio::PartitionAudio *value) { playback_ = value; }
  void set_dac(es8311::ES8311 *value) { dac_ = value; }
  void set_speaker(i2s_audio::I2SAudioSpeakerBase *value) { speaker_ = value; }
  void set_clock(voice_pyramid::Clock *value) { clock_ = value; }
  void set_amplifier(voice_pyramid::Amplifier *value) { amplifier_ = value; }
  ::openathan::SettingsStore *settings_store() { return &settings_store_; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override {
    if (is_failed()) return;
    if ((clock_ && clock_->is_failed()) || (amplifier_ && amplifier_->is_failed()) ||
        (dac_ && dac_->is_failed()) || (speaker_ && !speaker_->volume_fence_healthy())) {
      fail_(); return;
    }
    if (!hardware_healthy_()) {
      // Losing readiness during a transition cannot reset its fault deadline.
      if (applied_ || (deadline_ && esp_timer_get_time() >= *deadline_)) fail_();
      return;
    }
    if (!pending_) return;
    if (!deadline_) deadline_ = esp_timer_get_time() + 2000000;
    if (esp_timer_get_time() >= *deadline_) { fail_(); return; }
    if (phase_ == Phase::MUTING) {
      uint8_t gain{}, drc{};
      if (!amplifier_->set_pa_enabled(false) || !codec_mute_(true) ||
          !dac_->write_byte(0x34, 0) || !dac_->set_volume(plan_.codec_level) ||
          !dac_->read_byte(0x32, &gain) || gain != static_cast<uint8_t>(plan_.codec_level * 255) ||
          !dac_->read_byte(0x34, &drc) || drc != 0) { fail_(); return; }
      token_ = speaker_->request_volume_with_fence(plan_.software_level);
      phase_ = Phase::FENCING;
    }
    const auto status = speaker_->volume_fence_status(token_);
    if (status == i2s_audio::VolumeFenceStatus::FAILED) { fail_(); return; }
    if (status == i2s_audio::VolumeFenceStatus::CANCELLED) {
      // Stop or task restart invalidates the token. Re-request without starting
      // playback; an entirely cleaned-up speaker may acknowledge immediately.
      token_ = speaker_->request_volume_with_fence(plan_.software_level);
      return;
    }
    if (status != i2s_audio::VolumeFenceStatus::COMPLETE) return;
    if (*pending_ && (!codec_mute_(false) || !amplifier_->set_pa_enabled(true))) { fail_(); return; }
    // Zero is a valid applied setting with both hardware gates still closed.
    applied_ = pending_; pending_.reset(); phase_ = Phase::IDLE; deadline_.reset();
  }
  void on_shutdown() override {
    if (speaker_) { speaker_->request_volume_with_fence(0); speaker_->stop(); }
    if (dac_) codec_mute_(true);
    if (amplifier_) amplifier_->set_pa_enabled(false);
  }
  bool healthy() const override { return hardware_healthy_() && applied_.has_value(); }
  bool ready() const override { return healthy() && phase_ == Phase::IDLE; }
  bool playing() const override { return playback_ && playback_->playing(); }
  bool start(::openathan::Track track) override { return ready() && playback_->start(track); }
  bool stream_supported() const override { return playback_ && playback_->stream_supported(); }
  uint32_t playback_epoch() const override { return playback_ ? playback_->playback_epoch() : 0; }
  ::openathan::PlaybackSource source() const override { return playback_ ? playback_->source() : ::openathan::PlaybackSource::NONE; }
  ::openathan::StreamState stream_state() const override { return playback_ ? playback_->stream_state() : ::openathan::StreamState::IDLE; }
  bool start_stream(const std::string &url) override { return ready() && playback_->start_stream(url); }
  void stop() override {
    if (playback_) playback_->stop();
    if (speaker_) speaker_->stop();
  }
  std::optional<unsigned> volume_percent() const override {
    return ready() && std::fabs(speaker_->fenced_volume() - plan_.software_level) <= 0.00001f ? applied_ : std::nullopt;
  }
  bool request_volume_percent(unsigned percent) override {
    if (percent > 100 || is_failed()) return false;
    if (pending_ == percent || volume_percent() == percent) return true;
    pending_ = percent; plan_ = volume_plan(percent); phase_ = Phase::MUTING;
    deadline_.reset();  // A distinct latest request starts its own transition.
    // Close the verified gates synchronously before a superseded fence can
    // reopen output. loop() repeats this check before changing hardware gain.
    if (hardware_healthy_()) {
      deadline_ = esp_timer_get_time() + 2000000;
      if (!amplifier_->set_pa_enabled(false) || !codec_mute_(true)) { fail_(); return false; }
    }
    // Invalidate the old generation immediately; this zero request is never
    // permission to open either gate. Duplicate requests returned above retain
    // the current token and deadline; Stop/restart cancellation does too.
    if (speaker_) speaker_->request_volume_with_fence(0);
    return true;
  }
 private:
  enum class Phase { IDLE, MUTING, FENCING };
  bool codec_mute_(bool muted) {
    uint8_t value{};
    return (muted ? dac_->set_mute_on() : dac_->set_mute_off()) &&
        dac_->read_byte(0x31, &value) && (value & 0x60) == (muted ? 0x60 : 0);
  }
  void fail_() {
    if (speaker_) { speaker_->request_volume_with_fence(0); speaker_->stop(); }
    if (playback_) playback_->stop();
    if (dac_) codec_mute_(true);
    if (amplifier_) amplifier_->set_pa_enabled(false);
    applied_.reset(); mark_failed();
  }
  bool hardware_healthy_() const {
    return !is_failed() && clock_ && !clock_->is_failed() && amplifier_ && amplifier_->ready() &&
        dac_ && !dac_->is_failed() && playback_ && playback_->ready() && speaker_ && speaker_->volume_fence_healthy();
  }
  openathan_audio::PartitionAudio *playback_{};
  es8311::ES8311 *dac_{};
  i2s_audio::I2SAudioSpeakerBase *speaker_{};
  voice_pyramid::Clock *clock_{};
  voice_pyramid::Amplifier *amplifier_{};
  VolumeSettingsStore settings_store_;
  VolumePlan plan_{};
  std::optional<unsigned> applied_, pending_;
  std::optional<int64_t> deadline_;
  i2s_audio::VolumeFenceToken token_{};
  Phase phase_{Phase::IDLE};
};
}  // namespace esphome::voice_pyramid_audio
