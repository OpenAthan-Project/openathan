#pragma once
#include <cstdint>
#include <vector>
namespace esphome::i2c {
struct Write { uint8_t reg; std::vector<uint8_t> data; };
class I2CDevice {
 public:
  std::vector<Write> writes;
  unsigned fail_at{};
  bool write_bytes(uint8_t reg, const uint8_t *data, size_t size) {
    writes.push_back({reg,{data,data+size}});
    return !fail_at || writes.size()!=fail_at;
  }
  bool write_byte(uint8_t reg, uint8_t value) { return write_bytes(reg,&value,1); }
};
}
