#pragma once
#include <cstdint>
namespace esphome {
inline uint64_t decoder_yield_ms = 0;
inline void delay(uint32_t milliseconds) { decoder_yield_ms += milliseconds; }
}
