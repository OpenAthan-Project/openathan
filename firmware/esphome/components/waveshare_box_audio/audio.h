#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/es8311/es8311.h"
#include "esphome/components/openathan_audio/partition_audio.h"

namespace esphome::waveshare_box_audio {
// Keep power sequencing and codec health out of reusable prayer/product logic.
class Audio : public Component, public ::openathan::Playback {
 public:
  void set_playback(openathan_audio::PartitionAudio *value) { playback_ = value; }
  void set_dac(es8311::ES8311 *value) { dac_ = value; }
  void set_amplifier(GPIOPin *value) { amplifier_ = value; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override { amplifier_->setup(); amplifier_->digital_write(false); }
  void loop() override {
    if (!gain_configured_ && !is_failed() && dac_ && !dac_->is_failed() &&
        playback_ && playback_->ready()) {
      // ESPHome 2026.9.0 maps 0.75 to ES8311 register 0xBF (0 dB).
      // The speaker controls volume in software; never amplify the samples.
      if (dac_->set_volume(0.75f) && dac_->set_mute_off()) gain_configured_ = true;
      else mark_failed();
    }
    const bool enable = healthy_();
    if (enable != enabled_) { amplifier_->digital_write(enable); enabled_ = enable; }
  }
  void on_shutdown() override { amplifier_->digital_write(false); enabled_ = false; }
  bool ready() const override { return enabled_ && healthy_(); }
  bool playing() const override { return playback_ && playback_->playing(); }
  bool start(::openathan::Track track) override { return ready() && playback_->start(track); }
  void stop() override { if (playback_) playback_->stop(); }
  bool stream_supported() const override { return playback_ && playback_->stream_supported(); }
  uint32_t playback_epoch() const override { return playback_ ? playback_->playback_epoch() : 0; }
  ::openathan::PlaybackSource source() const override { return playback_ ? playback_->source() : ::openathan::PlaybackSource::NONE; }
  ::openathan::StreamState stream_state() const override { return playback_ ? playback_->stream_state() : ::openathan::StreamState::IDLE; }
  bool start_stream(const std::string &url) override { return ready() && playback_->start_stream(url); }
  std::optional<unsigned> volume_percent() const override {
    return healthy_() ? playback_->volume_percent() : std::nullopt;
  }
  bool request_volume_percent(unsigned percent) override {
    return healthy_() && playback_->request_volume_percent(percent);
  }
 private:
  bool healthy_() const {
    return !is_failed() && gain_configured_ && dac_ && !dac_->is_failed() &&
           playback_ && playback_->ready();
  }
  openathan_audio::PartitionAudio *playback_{};
  es8311::ES8311 *dac_{};
  GPIOPin *amplifier_{};
  bool enabled_{};
  bool gain_configured_{};
};
}
