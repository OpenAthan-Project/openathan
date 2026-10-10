#pragma once
#include "esphome/core/component.h"
#include <vector>
namespace esphome::es8311 {
class ES8311 : public Component {
 public:
  bool volume_write_ok{true};
  bool unmute_write_ok{true};
  unsigned unmute_writes{};
  std::vector<float> volumes;
  bool set_volume(float value) { volumes.push_back(value); return volume_write_ok; }
  bool set_mute_off() { ++unmute_writes; return unmute_write_ok; }
};
}
