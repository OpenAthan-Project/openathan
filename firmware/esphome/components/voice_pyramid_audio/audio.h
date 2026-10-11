#pragma once
#include "esphome/components/voice_pyramid/voice_pyramid.h"
#include "volume_store.h"
#include "esphome/components/es8311/es8311.h"
#include "esphome/components/openathan_audio/partition_audio.h"
#include "esphome/components/switch/switch.h"

namespace esphome::voice_pyramid_audio {
class Audio : public Component, public ::openathan::Playback {
 public:
  explicit Audio(std::string storage_namespace = openathan_storage::PRAYER) : settings_store_(std::move(storage_namespace)) {}
  void set_playback(openathan_audio::PartitionAudio *value) { playback_ = value; }
  void set_dac(es8311::ES8311 *value) { dac_ = value; }
  void set_enable(switch_::Switch *value) { enable_ = value; }
  void set_clock(voice_pyramid::Clock *value) { clock_ = value; }
  void set_amplifier(voice_pyramid::Amplifier *value) { amplifier_ = value; }
  ::openathan::SettingsStore *settings_store() { return &settings_store_; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override { enable_->turn_off(); }
  void loop() override {
    if (!hardware_healthy_() || phase_ != Phase::IDLE || !applied_) enable_->turn_off();
    if (!hardware_healthy_()) return;
    if (!pending_) {
      if (applied_ && phase_ == Phase::IDLE) enable_->turn_on();
      return;
    }
    const auto level = playback_->volume_level();
    if (phase_ == Phase::MUTING) {
      if (!level || *level != 0.0f) { playback_->request_volume_level(0.0f); return; }
      uint8_t gain{}, drc{};
      if (!dac_->set_mute_on() || !dac_->write_byte(0x34, 0x00) || !dac_->set_volume(plan_.codec_level) ||
          !dac_->read_byte(0x32, &gain) || gain != static_cast<uint8_t>(plan_.codec_level * 255) ||
          !dac_->read_byte(0x34, &drc) || drc != 0 ||
          (*pending_ && !dac_->set_mute_off())) {
        mark_failed(); return;
      }
      phase_ = Phase::APPLYING;
      playback_->request_volume_level(plan_.software_level);
    } else if (phase_ == Phase::APPLYING) {
      if (!level || std::fabs(*level - plan_.software_level) > 0.00001f) {
        playback_->request_volume_level(plan_.software_level); return;
      }
      applied_ = pending_; pending_.reset(); phase_ = Phase::IDLE;
      enable_->turn_on();
    }
  }
  void on_shutdown() override { enable_->turn_off(); dac_->set_mute_on(); }
  bool ready() const override { return hardware_healthy_() && phase_ == Phase::IDLE && applied_ && enable_->state; }
  bool playing() const override { return playback_ && playback_->playing(); }
  bool start(::openathan::Track track) override { return ready() && playback_->start(track); }
  void stop() override { if (playback_) playback_->stop(); }
  std::optional<unsigned> volume_percent() const override {
    if (!ready()) return {};
    const auto level = playback_->volume_level();
    return level && std::fabs(*level - plan_.software_level) <= 0.00001f ? applied_ : std::nullopt;
  }
  bool request_volume_percent(unsigned percent) override {
    if (percent > 100 || is_failed()) return false;
    if (pending_ == percent || volume_percent() == percent) return true;
    pending_ = percent; plan_ = volume_plan(percent); phase_ = Phase::MUTING;
    if (enable_) enable_->turn_off();
    if (playback_) playback_->request_volume_level(0.0f);
    return true;
  }
 private:
  enum class Phase { IDLE, MUTING, APPLYING };
  bool hardware_healthy_() const {
    return !is_failed() && clock_ && !clock_->is_failed() && amplifier_ && amplifier_->ready() &&
        dac_ && !dac_->is_failed() && playback_ && playback_->ready();
  }
  openathan_audio::PartitionAudio *playback_{};
  es8311::ES8311 *dac_{};
  switch_::Switch *enable_{};
  voice_pyramid::Clock *clock_{};
  voice_pyramid::Amplifier *amplifier_{};
  VolumeSettingsStore settings_store_;
  VolumePlan plan_{};
  std::optional<unsigned> applied_, pending_;
  Phase phase_{Phase::IDLE};
};
}  // namespace esphome::voice_pyramid_audio
