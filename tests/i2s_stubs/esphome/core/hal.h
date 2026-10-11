#pragma once
#include "fake_i2s.h"
#define IRAM_ATTR
namespace esphome {inline uint32_t millis(){return fake_i2s::now/1000;}}
