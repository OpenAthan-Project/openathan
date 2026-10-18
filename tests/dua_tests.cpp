#include "openathan/dua.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace openathan;
struct Memory : DuaStore {
  SavedDua value;
  LoadResult result{LoadResult::EMPTY}; bool fail{}; unsigned writes{};
  LoadResult load(SavedDua &v) override { v = value; return result; }
  bool save(const SavedDua &v) override { ++writes; if (fail) return false; value = v; result = LoadResult::LOADED; return true; }
};
struct Audio : Playback {
  bool ok{true}, installed{true}, active{}; unsigned volume{50}, starts{}; uint32_t epoch{};
  Track track{Track::NORMAL}; PlaybackCompletion done;
  bool ready() const override { return ok; }
  bool playing() const override { return active; }
  bool dua_ready() const override { return installed; }
  uint32_t playback_epoch() const override { return epoch; }
  PlaybackCompletion completion() const override { return done; }
  std::optional<unsigned> volume_percent() const override { return volume; }
  bool start(Track t) override { ++epoch; ++starts; track = t; active = ok; return ok; }
  void stop() override { done = {epoch, PlaybackOutcome::CANCELLED}; ++epoch; active = false; }
  void finish(PlaybackOutcome outcome = PlaybackOutcome::COMPLETED) { done = {epoch, outcome}; active = false; }
};
int main() {
  Memory store; DuaPreferences prefs(store); prefs.begin();
  assert(prefs.writable() && !prefs.enabled() && store.writes == 0);
  assert(prefs.update(true, 7) == DuaSaveResult::CONFLICT);
  assert(prefs.update(true, 1) == DuaSaveResult::SAVED && prefs.enabled());
  assert(prefs.update(true, 2) == DuaSaveResult::UNCHANGED && store.writes == 1);
  auto record = encode_dua(store.value); SavedDua decoded;
  assert(decode_dua(record, decoded) && decoded == store.value);
  for (size_t i = 0; i < record.size(); ++i) { auto bad = record; bad[i] ^= 1; assert(!decode_dua(bad, decoded)); }
  store.fail = true; assert(prefs.update(false, 2) == DuaSaveResult::STORAGE && !prefs.enabled());
  store.result = LoadResult::ERROR; prefs.begin(); assert(!prefs.enabled() && !prefs.writable());
  for (auto track : {Track::NORMAL, Track::FAJR}) {
    Audio audio; DuaSequence seq; seq.set_backend(&audio); seq.set_enabled(true);
    seq.tick(1000); assert(seq.start(track)); audio.finish(); seq.tick(1100);
    assert(seq.phase() == DuaSequence::Phase::PAUSE && seq.busy() && !seq.playing());
    seq.tick(3099); assert(audio.starts == 1); seq.tick(3100);
    assert(audio.starts == 2 && audio.track == Track::DUA); audio.finish(); seq.tick(3200);
    assert(!seq.busy()); seq.tick(3300); assert(audio.starts == 2);
  }
  for (unsigned variant = 0; variant < 10; ++variant) {
    Audio audio; DuaSequence seq; seq.set_backend(&audio); seq.set_enabled(true); seq.tick(1000);
    if (variant == 0) audio.volume = 0;
    if (variant == 1) audio.installed = false;
    assert(seq.start(Track::NORMAL));
    if (variant == 2) seq.stop();
    if (variant == 3) seq.set_enabled(false);
    if (variant == 4) audio.volume = 0;
    if (variant == 5) audio.stop();
    if (variant == 6) audio.ok = false;
    audio.finish(variant == 7 ? PlaybackOutcome::FAILED : PlaybackOutcome::COMPLETED);
    if (variant == 8) { audio.done.epoch = 999; seq.tick(1100); assert(seq.phase() == DuaSequence::Phase::ATHAN); seq.stop(); }
    if (variant == 9) { seq.tick(1100); seq.tick(6000); }
    else seq.tick(1100);
    seq.tick(3000); seq.tick(4000); assert(audio.starts == 1);
  }
  Audio audio; DuaSequence seq; seq.set_backend(&audio); seq.tick(1000); assert(seq.start(Track::NORMAL));
  seq.set_enabled(true); audio.finish(); seq.tick(1100); seq.tick(3100); assert(audio.starts == 1);
  seq.tick(3200); assert(seq.start(Track::FAJR)); audio.finish(); seq.tick(3300); seq.set_enabled(false);
  seq.tick(5300); assert(audio.starts == 2);
  std::cout << "Dua preference and sequence scenarios passed\n";
}
