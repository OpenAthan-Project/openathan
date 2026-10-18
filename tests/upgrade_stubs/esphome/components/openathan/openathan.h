#pragma once
#include "esphome/components/json/json_util.h"
#include "esphome/components/openathan/storage_config.h"
#include <optional>
#include "openathan/scheduler.h"
#include "openathan/dua.h"
namespace esphome::openathan_component {
struct Settings { bool healthy() const { return true; } };
struct Clock { bool valid{true}; int64_t utc{1000}; };
struct Next { int64_t utc{9000}; };
struct Status { bool playing{}, automatic_ready{true}; std::optional<Next> next{Next{}}; ::openathan::Fault fault{::openathan::Fault::NONE}; };
struct DuaMemory : ::openathan::DuaStore {
  ::openathan::SavedDua value;
  ::openathan::LoadResult load(::openathan::SavedDua &out) override { out=value; return ::openathan::LoadResult::LOADED; }
  bool save(const ::openathan::SavedDua &out) override { value=out; return true; }
};
class OpenAthan {
 public:
  DuaMemory dua_store;
  ::openathan::DuaPreferences dua_prefs{dua_store};
  ::openathan::DuaSequence sequence;
  const ::openathan::DuaPreferences &dua_preferences() const { return dua_prefs; }
  const ::openathan::DuaSequence &dua_sequence() const { return sequence; }
  bool health{true}, active{true}, ready{true}, failed{};
  Clock clock;
  Status sample;
  Settings settings;
  ::openathan::Playback *audio{};
  ::openathan::Playback *playback() const { return audio; }
  void stop() { if (audio) audio->stop(); }
  Status status() const {return sample;}
  Clock read() const {return clock;}
  Settings *settings_service(){return &settings;}
  bool playback_busy() const { return sample.playing || (audio && audio->busy()); }
  bool activated()const{return active;}
  bool is_failed()const{return failed;}
  bool upgrade_health()const{return health;}
  const char *setup_state()const{return "active";}
};
}
