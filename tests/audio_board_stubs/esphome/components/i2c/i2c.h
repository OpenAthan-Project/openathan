#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
namespace esphome::i2c {
class I2CDevice {
 public:
  std::array<uint8_t, 8> regs{0xff,0xff,0xff,0xff,0,0,0xff,0xff};
  std::vector<std::pair<uint8_t,uint8_t>> writes;
  unsigned fail_at{};
  bool fail_read{}, bad_readback{};
  bool read_byte(uint8_t reg, uint8_t *value) {
    if (fail_read) return false;
    *value = regs[reg] ^ (bad_readback && reg == 3 ? 1 : 0); return true;
  }
  bool write_byte(uint8_t reg, uint8_t value) {
    writes.push_back({reg,value});
    if (fail_at && writes.size()==fail_at) return false;
    regs[reg]=value; return true;
  }
};
}
