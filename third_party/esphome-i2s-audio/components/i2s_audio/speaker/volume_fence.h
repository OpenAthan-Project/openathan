// OpenAthan additions, 2026-10-11. GPL-3.0-only; see ../../../LICENSE.
#pragma once
#include <cstdint>
#include <limits>

namespace esphome::i2s_audio {
struct VolumeFenceToken {
  uint32_t session{}, generation{};
  bool operator==(const VolumeFenceToken &other) const {
    return session == other.session && generation == other.generation;
  }
};
enum class VolumeFenceStatus { PENDING, COMPLETE, CANCELLED, FAILED };
struct VolumeGain {
  VolumeFenceToken token;
  int32_t factor{};
};
struct VolumeDmaRecord {
  uint32_t real_frames{};
  VolumeFenceToken token;
};

// Caller holds the speaker's critical section. This state allocates no memory.
// One gain snapshot covers a complete DMA descriptor, whose record travels in
// the same FIFO as its completion. No timer or logical player state completes it.
class VolumeFenceState {
 public:
  VolumeFenceToken request(float level, int32_t factor) {
    if (failed_ || !advance_(generation_)) return {};
    cancelled_ = false;
    level_ = level;
    factor_ = factor;
    completed_ = active_ ? 0 : generation_;
    return {session_, generation_};
  }
  VolumeFenceStatus status(VolumeFenceToken token) const {
    if (failed_ || !token.generation) return VolumeFenceStatus::FAILED;
    if (cancelled_ || !(token == snapshot().token)) return VolumeFenceStatus::CANCELLED;
    return completed_ == generation_ ? VolumeFenceStatus::COMPLETE : VolumeFenceStatus::PENDING;
  }
  bool begin_session() {
    if (failed_ || !advance_(session_)) return false;
    cancelled_ = false;
    active_ = true;
    completed_ = 0;
    return true;
  }
  void end_session() {
    if (!active_) return;
    active_ = false;
    if (advance_(session_)) completed_ = generation_;
  }
  void complete(VolumeFenceToken token) {
    if (!failed_ && token == snapshot().token) completed_ = generation_;
  }
  void cancel() { cancelled_ = true; }
  bool active() const { return active_; }
  void fail() { failed_ = true; factor_ = 0; level_ = 0; }
  bool failed() const { return failed_; }
  VolumeGain snapshot() const { return {{session_, generation_}, factor_}; }
  float level() const { return level_; }
 private:
  bool advance_(uint32_t &counter) {
    if (counter == std::numeric_limits<uint32_t>::max()) { fail(); return false; }
    ++counter;
    return true;
  }
  uint32_t session_{}, generation_{}, completed_{};
  int32_t factor_{};
  float level_{};
  friend struct VolumeFenceTestAccess;
  bool active_{}, failed_{}, cancelled_{};
};
}  // namespace esphome::i2s_audio
