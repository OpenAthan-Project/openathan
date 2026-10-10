#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace esphome::i2c {
struct Write { uint8_t reg; std::vector<uint8_t> data; };
class I2CDevice {
 public:
  std::array<uint8_t, 18> registers{};
  std::vector<Write> writes;
  unsigned reads{}, fail_read_at{}, fail_write_at{};
  std::function<void(unsigned, uint8_t *)> on_read;
  bool read_bytes(uint8_t reg, uint8_t *data, size_t size) {
    ++reads;
    if (reads == fail_read_at) return false;
    for (size_t i = 0; i < size; ++i) data[i] = registers.at(reg + i);
    if (on_read) on_read(reads, data);
    return true;
  }
  bool write_bytes(uint8_t reg, const uint8_t *data, size_t size) {
    writes.push_back({reg, {data, data + size}});
    const bool failed = writes.size() == fail_write_at;
    // A failed multi-byte transfer may have changed a prefix of the registers.
    for (size_t i = 0; i < (failed ? size / 2 : size); ++i) registers.at(reg + i) = data[i];
    return !failed;
  }
  bool write_byte(uint8_t reg, uint8_t value) { return write_bytes(reg, &value, 1); }
};
}
