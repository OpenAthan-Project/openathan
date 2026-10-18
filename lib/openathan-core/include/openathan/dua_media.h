#pragma once
#include "audio_image.h"
#include <limits>

namespace openathan {
constexpr size_t DUA_BANK_START = 0x300000;
constexpr size_t DUA_BANK_SIZE = 0x40000;
constexpr size_t DUA_HEADER_SIZE = 4096;
constexpr size_t DUA_MAX_BYTES = DUA_BANK_SIZE - DUA_HEADER_SIZE;
constexpr size_t DUA_ENVELOPE_MAX = 2048;
constexpr uint32_t DUA_COMMIT = 0x314d4f43;
struct DuaBank {
  unsigned index{};
  uint32_t generation{}, bytes{};
  const uint8_t *payload{}, *digest{};
  const char *envelope{};
  uint32_t envelope_size{};
};
inline bool dua_bank_recognized(const uint8_t *bank) {
  return std::memcmp(bank, "OADUA001", 8) == 0;
}
inline bool dua_bank_erased(const uint8_t *bank) {
  for (size_t i = 0; i < DUA_BANK_SIZE; ++i) if (bank[i] != 0xff) return false;
  return true;
}
// Signature verifier must bind the envelope to these exact bytes and digest.
template<typename HashMatches, typename Verify>
bool validate_dua_bank(const uint8_t *data, unsigned index, HashMatches hash, Verify verify, DuaBank &out) {
  if (!data || index > 1 || !dua_bank_recognized(data) || read_u32(data + 4092) != DUA_COMMIT ||
      read_u32(data + 20) != 1 || !read_u32(data + 8) ||
      !hash(data, 4060, data + 4060)) return false;
  const auto length = read_u32(data + 12), envelope_size = read_u32(data + 16);
  if (!length || length > DUA_MAX_BYTES || !envelope_size || envelope_size > DUA_ENVELOPE_MAX ||
      !verify(reinterpret_cast<const char *>(data + 64), envelope_size, length, data + 24) ||
      !hash(data + DUA_HEADER_SIZE, length, data + 24)) return false;
  out = {index, read_u32(data + 8), length, data + DUA_HEADER_SIZE, data + 24,
         reinterpret_cast<const char *>(data + 64), envelope_size};
  return true;
}
}  // namespace openathan
