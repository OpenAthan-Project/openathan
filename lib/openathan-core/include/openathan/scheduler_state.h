#pragma once
#include "scheduler.h"

namespace openathan {
// Stable byte encoding, independent of compiler padding and std::optional ABI.
using StateRecord = std::array<uint8_t, 36>;
StateRecord encode_state(const DurableState &state);
bool decode_state(const StateRecord &record, DurableState &state);
}  // namespace openathan
