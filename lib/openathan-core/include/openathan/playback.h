#pragma once
#include "audio_image.h"

namespace openathan {
class Playback {
 public:
  virtual ~Playback() = default;
  virtual bool ready() const = 0;
  virtual bool playing() const = 0;
  // Replace current audio; true means the request was accepted, not audibly played.
  virtual bool start(Track track) = 0;
  virtual void stop() = 0;
};
}  // namespace openathan
