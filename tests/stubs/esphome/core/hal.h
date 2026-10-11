#pragma once
#include <cstdint>
namespace esphome { inline void delayMicroseconds(uint32_t) {} }
namespace esphome { inline uint32_t mock_millis{}; inline uint32_t millis() { return mock_millis; } }
