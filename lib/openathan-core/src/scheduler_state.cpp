#include "openathan/scheduler_state.h"
#include <cstring>

namespace openathan {
static uint32_t checksum(const uint8_t *bytes, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
  }
  return ~crc;
}
static void write_u32(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(value >> (8 * i));
}
StateRecord encode_state(const DurableState &state) {
  StateRecord record{};
  std::memcpy(record.data(), "OAS1", 4);
  for (size_t i = 0; i < 5; ++i) write_u32(record.data() + 4 + i * 4, uint32_t(state.consumed_through[i]));
  if (state.skip) {
    record[24] = 1;
    record[25] = static_cast<uint8_t>(state.skip->prayer);
    write_u32(record.data() + 28, uint32_t(state.skip->day));
  }
  write_u32(record.data() + 32, checksum(record.data(), 32));
  return record;
}
bool decode_state(const StateRecord &record, DurableState &out) {
  out = {};
  if (std::memcmp(record.data(), "OAS1", 4) || record[24] > 1 || record[25] > 4 ||
      record[26] || record[27] || read_u32(record.data() + 32) != checksum(record.data(), 32)) return false;
  DurableState candidate;
  const auto valid_serial = [](int32_t serial) {
    return serial >= day_number({1900, 1, 1}) - 2 && serial <= day_number({2100, 12, 31}) + 1;
  };
  for (size_t i = 0; i < 5; ++i) {
    const int32_t serial = static_cast<int32_t>(read_u32(record.data() + 4 + i * 4));
    if (serial != NEVER_CONSUMED && !valid_serial(serial)) return false;
    candidate.consumed_through[i] = serial;
  }
  if (record[24]) {
    const int32_t serial = static_cast<int32_t>(read_u32(record.data() + 28));
    if (!valid_serial(serial) || serial <= candidate.consumed_through[record[25]]) return false;
    candidate.skip = EventKey{serial, static_cast<Prayer>(record[25])};
  } else if (record[25] || read_u32(record.data() + 28)) return false;
  out = candidate;
  return true;
}
}  // namespace openathan
