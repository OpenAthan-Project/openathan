#pragma once
#include <cassert>
#include <cstdint>
#include <cstdlib>
#define MALLOC_CAP_8BIT (1U << 2)
#define MALLOC_CAP_INTERNAL (1U << 11)
inline bool read_buffer_allocation_fails{};
inline unsigned read_buffer_allocation_attempts{}, live_read_buffers{};
inline size_t last_read_buffer_bytes{};
inline uint32_t last_read_buffer_caps{};
inline void *heap_caps_malloc(size_t bytes, uint32_t caps) {
  ++read_buffer_allocation_attempts;
  last_read_buffer_bytes = bytes; last_read_buffer_caps = caps;
  if (read_buffer_allocation_fails) return nullptr;
  auto *buffer = std::malloc(bytes);
  if (buffer) ++live_read_buffers;
  return buffer;
}
inline void heap_caps_free(void *buffer) {
  if (buffer) { assert(live_read_buffers); --live_read_buffers; std::free(buffer); }
}
#define MALLOC_CAP_SPIRAM (1U << 10)
inline size_t heap_caps_get_free_size(unsigned){return 65536;}
inline size_t heap_caps_get_largest_free_block(unsigned){return 32768;}
inline size_t heap_caps_get_minimum_free_size(unsigned){return 16384;}
