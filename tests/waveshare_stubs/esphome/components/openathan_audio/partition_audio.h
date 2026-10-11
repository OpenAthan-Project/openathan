#pragma once
#include "openathan/playback.h"
#include <cmath>
#include <vector>
namespace esphome::openathan_audio {
class PartitionAudio : public ::openathan::Playback {
 public:
  bool available{}, active{};
  unsigned starts{}, stops{}, volume{70};
  float level{0.7f};
  bool apply_requests{true};
  uint32_t epoch{};
  ::openathan::PlaybackSource kind{::openathan::PlaybackSource::NONE};
  std::vector<float> requests;
  bool ready() const override { return available; }
  bool playing() const override { return active; }
  bool start(::openathan::Track) override { ++starts; ++epoch; kind = ::openathan::PlaybackSource::ATHAN; return active = true; }
  void stop() override { ++stops; ++epoch; active = false; kind = ::openathan::PlaybackSource::NONE; }
  bool stream_supported() const override { return true; }
  uint32_t playback_epoch() const override { return epoch; }
  ::openathan::PlaybackSource source() const override { return kind; }
  ::openathan::StreamState stream_state() const override { return active && kind == ::openathan::PlaybackSource::QURAN ? ::openathan::StreamState::PLAYING : ::openathan::StreamState::IDLE; }
  bool start_stream(const std::string &) override { ++starts; ++epoch; kind = ::openathan::PlaybackSource::QURAN; return active = true; }
  std::optional<unsigned> volume_percent() const override { return volume; }
  bool request_volume_percent(unsigned percent) override { volume = percent; return request_volume_level(percent / 100.0f); }
  std::optional<float> volume_level() const { return available ? std::optional<float>(level) : std::nullopt; }
  bool request_volume_level(float value) {
    if (!available || !std::isfinite(value) || value < 0 || value > 1) return false;
    requests.push_back(value);
    if (apply_requests) level = value;
    return true;
  }
};
}
