#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
namespace esphome::i2c {
struct Write { uint8_t reg; std::vector<uint8_t> data; };
class I2CDevice {
 public:
  std::vector<Write> writes;
  unsigned fail_at{};
  bool fail_read{};
  bool write_bytes(uint8_t reg, const uint8_t *data, size_t size) {
    writes.push_back({reg,{data,data+size}});
    return !fail_at || writes.size()!=fail_at;
  }
  bool write_byte(uint8_t reg, uint8_t value) { return write_bytes(reg,&value,1); }
  bool read_bytes(uint8_t reg, uint8_t *data, size_t size) {
    if (fail_read) return false;
    for (auto it = writes.rbegin(); it != writes.rend(); ++it) {
      if (it->reg == reg && it->data.size() == size) {
        std::copy(it->data.begin(), it->data.end(), data); return true;
      }
    }
    return false;
  }
  bool read_byte(uint8_t reg, uint8_t *value) { return read_bytes(reg, value, 1); }
};
}
