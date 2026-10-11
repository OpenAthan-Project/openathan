#pragma once
#include "esphome/core/component.h"
#include <vector>
#include <cstdint>
namespace esphome::es8311 {
class ES8311 : public Component {
 public:
  bool volume_write_ok{true};
  bool unmute_write_ok{true};
  bool mute_write_ok{true};
  unsigned unmute_writes{};
  unsigned mute_writes{};
  bool register_write_ok{true}, register_read_ok{true}, wrong_readback{};
  uint8_t gain{}, drc{};
  std::vector<float> volumes;
  bool set_volume(float value) { volumes.push_back(value); gain = static_cast<uint8_t>(value * 255); return volume_write_ok; }
  bool set_mute_off() { ++unmute_writes; return unmute_write_ok; }
  bool set_mute_on() { ++mute_writes; return mute_write_ok; }
  bool write_byte(uint8_t reg, uint8_t value) { if (reg == 0x34) drc = value; return register_write_ok; }
  bool read_byte(uint8_t reg, uint8_t *value) {
    *value = (reg == 0x32 ? gain : drc) ^ (wrong_readback ? 1 : 0);
    return register_read_ok;
  }
};
}
