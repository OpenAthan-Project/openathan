#pragma once
#include "audio_image.h"
#include <optional>
#include <string>

namespace openathan {
enum class PlaybackSource { NONE, ATHAN, QURAN };
enum class StreamState { IDLE, LOADING, PLAYING, ERROR };
class Playback {
 public:
  virtual ~Playback() = default;
  virtual bool ready() const = 0;
  virtual bool playing() const = 0;
  // Replace current audio; true means the request was accepted, not audibly played.
  virtual bool start(Track track) = 0;
  virtual void stop() = 0;
  // Optional streaming capability. Epoch changes revoke outstanding requests.
  virtual bool stream_supported() const { return false; }
  virtual uint32_t playback_epoch() const { return 0; }
  virtual PlaybackSource source() const { return playing() ? PlaybackSource::ATHAN : PlaybackSource::NONE; }
  virtual StreamState stream_state() const { return StreamState::IDLE; }
  virtual bool start_stream(const std::string &) { return false; }
  // Optional volume capability used by runtime settings, never by calculations.
  // Readback must report the applied control level, not a queued request.
  virtual std::optional<unsigned> volume_percent() const { return {}; }
  virtual bool request_volume_percent(unsigned percent) { return false; }
};
}  // namespace openathan
