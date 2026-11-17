#pragma once
#include <cstdlib>
inline void heap_caps_free(void *buffer) { std::free(buffer); }
