#pragma once
#include "settings.h"

namespace openathan {
struct SavedDua {
  uint32_t revision{1};
  bool enabled{};
  bool operator==(const SavedDua &) const = default;
};
using DuaRecord = std::array<uint8_t, 16>;
DuaRecord encode_dua(const SavedDua &value);
bool decode_dua(const DuaRecord &record, SavedDua &value);
class DuaStore {
 public:
  virtual ~DuaStore() = default;
  virtual LoadResult load(SavedDua &value) = 0;
  virtual bool save(const SavedDua &value) = 0;
};
enum class DuaSaveResult { SAVED, UNCHANGED, CONFLICT, STORAGE };
class DuaPreferences {
 public:
  explicit DuaPreferences(DuaStore &store) : store_(store) {}
  void begin();
  DuaSaveResult update(bool enabled, uint32_t revision);
  bool enabled() const { return writable_ && saved_ && saved_->enabled; }
  bool writable() const { return writable_; }
  const std::optional<SavedDua> &saved() const { return saved_; }
 private:
  DuaStore &store_;
  std::optional<SavedDua> saved_;
  bool writable_{};
};

// Only the scheduler starts Athan through this facade. Direct adapter previews
// cannot arm a follow-up. There is deliberately no durable playback sequence.
class DuaSequence : public Playback {
 public:
  enum class Phase { IDLE, ATHAN, PAUSE, DUA };
  void set_backend(Playback *backend) { backend_ = backend; }
  void set_enabled(bool enabled);
  void tick(uint64_t now);
  void cancel_followup();
  Phase phase() const { return phase_; }
  bool failed() const { return failed_; }
  bool ready() const override { return backend_ && backend_->ready(); }
  bool healthy() const override { return backend_ && backend_->healthy(); }
  bool playing() const override { return backend_ && backend_->playing(); }
  bool busy() const override { return phase_ != Phase::IDLE || (backend_ && backend_->busy()); }
  bool start(Track track) override;
  void stop() override;
  uint32_t playback_epoch() const override { return backend_ ? backend_->playback_epoch() : 0; }
  PlaybackCompletion completion() const override { return backend_ ? backend_->completion() : PlaybackCompletion{}; }
  PlaybackSource source() const override {
    return phase_ == Phase::PAUSE || phase_ == Phase::DUA ? PlaybackSource::DUA :
        backend_ ? backend_->source() : PlaybackSource::NONE;
  }
  bool dua_supported() const override { return backend_ && backend_->dua_supported(); }
  bool dua_ready() const override { return backend_ && backend_->dua_ready(); }
  bool set_dua_data(const uint8_t *data, size_t size) override {
    return backend_ && backend_->set_dua_data(data, size);
  }
  bool stream_supported() const override { return backend_ && backend_->stream_supported(); }
  StreamState stream_state() const override { return backend_ ? backend_->stream_state() : StreamState::IDLE; }
  bool start_stream(const std::string &url) override { return phase_ == Phase::IDLE && backend_ && backend_->start_stream(url); }
  std::optional<unsigned> volume_percent() const override { return backend_ ? backend_->volume_percent() : std::nullopt; }
  bool request_volume_percent(unsigned percent) override {
    if (!percent) cancel_followup();
    return backend_ && backend_->request_volume_percent(percent);
  }
 private:
  Playback *backend_{};
  Phase phase_{Phase::IDLE};
  bool enabled_{}, failed_{};
  uint32_t epoch_{};
  uint64_t last_tick_{}, pause_at_{};
};
const char *dua_phase_name(DuaSequence::Phase phase);
}  // namespace openathan
