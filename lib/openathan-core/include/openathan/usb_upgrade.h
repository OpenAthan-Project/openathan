#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openathan::usb_upgrade {
// OATHAN v1 additive commands. Improv and credential RPCs remain unchanged.
constexpr uint8_t INFO = 0x10, BEGIN = 0x11, VERIFY = 0x12, FINISH = 0x13, ABORT = 0x14;
constexpr uint8_t DATA = 5, ACK = 6;
constexpr size_t HEADER = 13, CHUNK = 255 - HEADER;
struct Chunk { uint64_t token; uint8_t kind; uint32_t offset; std::span<const uint8_t> bytes; };
inline std::optional<Chunk> decode(std::span<const uint8_t> bytes) {
  if (bytes.size() <= HEADER || bytes.size() > 255 || bytes[8] > 1) return {};
  uint64_t token = 0; uint32_t offset = 0;
  for (unsigned i = 0; i < 8; ++i) token |= uint64_t(bytes[i]) << (8 * i);
  for (unsigned i = 0; i < 4; ++i) offset |= uint32_t(bytes[9 + i]) << (8 * i);
  if (!token) return {};
  return Chunk{token, bytes[8], offset, bytes.subspan(HEADER)};
}
inline std::vector<uint8_t> ack(const Chunk &chunk) {
  std::vector<uint8_t> out(HEADER);
  for (unsigned i = 0; i < 8; ++i) out[i] = uint8_t(chunk.token >> (8 * i));
  out[8] = chunk.kind;
  const uint32_t next = chunk.offset + chunk.bytes.size();
  for (unsigned i = 0; i < 4; ++i) out[9 + i] = uint8_t(next >> (8 * i));
  return out;
}
}  // namespace openathan::usb_upgrade
