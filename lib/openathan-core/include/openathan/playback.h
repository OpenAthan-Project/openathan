#pragma once
#include "audio_image.h"
#include <optional>

namespace openathan {
class Playback {
 public:
  virtual ~Playback() = default;
  virtual bool ready() const = 0;
  virtual bool playing() const = 0;
  // Replace current audio; true means the request was accepted, not audibly played.
  virtual bool start(Track track) = 0;
  virtual void stop() = 0;
  // Optional volume capability used by runtime settings, never by calculations.
  // Readback must report the applied control level, not a queued request.
  virtual std::optional<unsigned> volume_percent() const { return {}; }
  virtual bool request_volume_percent(unsigned percent) { return false; }
};
}  // namespace openathan
