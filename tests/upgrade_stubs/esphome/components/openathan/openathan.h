#pragma once
#include "esphome/components/json/json_util.h"
#include "esphome/components/openathan/storage_config.h"
#include <optional>
#include "openathan/scheduler.h"
namespace esphome::openathan_component {
struct Settings { bool healthy() const { return true; } };
struct Clock { bool valid{true}; int64_t utc{1000}; };
struct Next { int64_t utc{9000}; };
struct Status { bool playing{}, automatic_ready{true}; std::optional<Next> next{Next{}}; ::openathan::Fault fault{::openathan::Fault::NONE}; };
class OpenAthan {
 public:
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
  bool activated()const{return active;}
  bool is_failed()const{return failed;}
  bool upgrade_health()const{return health;}
  const char *setup_state()const{return "active";}
};
}
