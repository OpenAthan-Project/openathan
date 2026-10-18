#include "openathan/dua.h"
#include <limits>

namespace openathan {
namespace {
uint32_t crc(const DuaRecord &r) {
  uint32_t v = 0xffffffff;
  for (size_t i = 0; i < 12; ++i) {
    v ^= r[i];
    for (unsigned b = 0; b < 8; ++b) v = (v >> 1) ^ ((v & 1) ? 0xedb88320 : 0);
  }
  return ~v;
}
void put(DuaRecord &r, size_t at, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) r[at + i] = v >> (i * 8);
}
}
DuaRecord encode_dua(const SavedDua &v) {
  DuaRecord r{'O', 'A', 'D', 1};
  put(r, 4, v.revision); r[8] = v.enabled; put(r, 12, crc(r)); return r;
}
bool decode_dua(const DuaRecord &r, SavedDua &out) {
  if (!read_u32(r.data() + 4) || r[8] > 1 || read_u32(r.data() + 12) != crc(r)) return false;
  SavedDua v{read_u32(r.data() + 4), r[8] == 1};
  if (encode_dua(v) != r) return false;
  out = v; return true;
}
void DuaPreferences::begin() {
  saved_.reset(); writable_ = false;
  SavedDua v;
  const auto result = store_.load(v);
  if (result == LoadResult::ERROR || !v.revision) return;
  saved_ = result == LoadResult::EMPTY ? SavedDua{} : v; writable_ = true;
}
DuaSaveResult DuaPreferences::update(bool enabled, uint32_t revision) {
  if (!writable_ || !saved_) return DuaSaveResult::STORAGE;
  if (saved_->revision != revision || revision == std::numeric_limits<uint32_t>::max()) return DuaSaveResult::CONFLICT;
  if (enabled == saved_->enabled) return DuaSaveResult::UNCHANGED;
  SavedDua v{revision + 1, enabled};
  if (!store_.save(v)) { writable_ = false; return DuaSaveResult::STORAGE; }
  saved_ = v; return DuaSaveResult::SAVED;
}
void DuaSequence::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled) cancel_followup();
}
void DuaSequence::cancel_followup() {
  if (phase_ == Phase::DUA && backend_) backend_->stop();
  phase_ = Phase::IDLE;
}
void DuaSequence::stop() {
  phase_ = Phase::IDLE;
  if (backend_) backend_->stop();
}
bool DuaSequence::start(Track track) {
  if (track != Track::NORMAL && track != Track::FAJR) return false;
  cancel_followup(); failed_ = false;
  if (!backend_ || !backend_->start(track)) return false;
  epoch_ = backend_->playback_epoch();
  const auto volume = backend_->volume_percent();
  if (enabled_ && backend_->dua_ready() && volume && *volume) phase_ = Phase::ATHAN;
  return true;
}
void DuaSequence::tick(uint64_t now) {
  const bool discontinuity = now < last_tick_ || (last_tick_ && now - last_tick_ > 3000);
  last_tick_ = now;
  if (phase_ == Phase::IDLE || !backend_) return;
  const auto volume = backend_->volume_percent();
  const auto done = backend_->completion();
  if (phase_ != Phase::PAUSE && done.epoch == epoch_ && done.outcome != PlaybackOutcome::NONE &&
      done.outcome != PlaybackOutcome::COMPLETED) {
    failed_ = done.outcome == PlaybackOutcome::FAILED; phase_ = Phase::IDLE; return;
  }
  if (discontinuity || !enabled_ || !backend_->healthy() || (volume && !*volume) ||
      backend_->playback_epoch() != epoch_) { cancel_followup(); return; }
  if (phase_ == Phase::PAUSE) {
    if (now - pause_at_ < 2000) return;
    if (!volume || !backend_->ready() || !backend_->dua_ready() || !backend_->start(Track::DUA)) {
      failed_ = true; phase_ = Phase::IDLE; return;
    }
    epoch_ = backend_->playback_epoch(); phase_ = Phase::DUA; return;
  }
  if (done.epoch != epoch_ || done.outcome == PlaybackOutcome::NONE) return;
  if (done.outcome != PlaybackOutcome::COMPLETED) {
    failed_ = done.outcome == PlaybackOutcome::FAILED; phase_ = Phase::IDLE; return;
  }
  if (phase_ == Phase::ATHAN) { phase_ = Phase::PAUSE; pause_at_ = now; }
  else phase_ = Phase::IDLE;
}
const char *dua_phase_name(DuaSequence::Phase phase) {
  switch (phase) {
    case DuaSequence::Phase::ATHAN: return "athan";
    case DuaSequence::Phase::PAUSE: return "pause";
    case DuaSequence::Phase::DUA: return "dua";
    default: return "idle";
  }
}
}  // namespace openathan
