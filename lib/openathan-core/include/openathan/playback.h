#pragma once
#include "audio_image.h"
#include <optional>
#include <string>

namespace openathan {
enum class PlaybackSource { NONE, ATHAN, QURAN, DUA };
enum class PlaybackOutcome { NONE, COMPLETED, CANCELLED, FAILED };
struct PlaybackCompletion {
  uint32_t epoch{};
  PlaybackOutcome outcome{PlaybackOutcome::NONE};
};
enum class StreamState { IDLE, LOADING, PLAYING, ERROR };
class Playback {
 public:
  virtual ~Playback() = default;
  virtual bool ready() const = 0;
  // Active playback health may remain true while new starts are temporarily gated.
  virtual bool healthy() const { return ready(); }
  virtual bool playing() const = 0;
  virtual bool busy() const { return playing() || source() != PlaybackSource::NONE || stream_state() == StreamState::LOADING; }
  virtual PlaybackCompletion completion() const { return {}; }
  virtual bool dua_supported() const { return false; }
  virtual bool dua_ready() const { return false; }
  // Optional product media binding. Caller retains immutable bytes until idle.
  virtual bool set_dua_data(const uint8_t *, size_t) { return false; }
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
