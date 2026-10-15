#include "phone_setup.h"
#include <algorithm>

namespace esphome::openathan_device::phone_setup {
size_t dns_response(std::span<const uint8_t> query, std::span<uint8_t> output) {
  if (query.size() < 17 || query.size() > 512 || (query[2] & 0xF8) || query[3] & 0x0F ||
      query[4] || query[5] != 1 || query[6] || query[7] || query[8] || query[9]) return 0;
  size_t end = 12;
  while (end < query.size() && query[end]) {
    const auto length = query[end++];
    if (length > 63 || end + length >= query.size() || end + length > 267) return 0;
    end += length;
  }
  if (end + 5 > query.size()) return 0;
  ++end;
  const unsigned type = unsigned(query[end]) * 256 + query[end + 1];
  if (query[end + 2] || query[end + 3] != 1) return 0;
  end += 4;
  const bool address = type == 1 || type == 255;
  const size_t length = end + (address ? 16 : 0);
  if (output.size() < length) return 0;
  std::copy_n(query.data(), end, output.data());
  output[2] = 0x80 | (query[2] & 1); output[3] = 0x80;
  output[6] = 0; output[7] = address ? 1 : 0;
  std::fill(output.begin() + 8, output.begin() + 12, 0);
  if (address) {
    constexpr uint8_t answer[]{0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4, 192, 168, 4, 1};
    std::copy_n(answer, sizeof(answer), output.data() + end);
  }
  return length;
}
}  // namespace esphome::openathan_device::phone_setup
