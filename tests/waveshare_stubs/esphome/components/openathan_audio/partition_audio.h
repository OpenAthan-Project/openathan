#pragma once
#include "openathan/playback.h"
namespace esphome::openathan_audio {
class PartitionAudio : public ::openathan::Playback {
 public:
  bool available{}, active{};
  unsigned starts{}, stops{}, volume{70};
  bool ready() const override { return available; }
  bool playing() const override { return active; }
  bool start(::openathan::Track) override { ++starts; return active = true; }
  void stop() override { ++stops; active = false; }
  std::optional<unsigned> volume_percent() const override { return volume; }
  bool request_volume_percent(unsigned percent) override { volume = percent; return true; }
};
}
