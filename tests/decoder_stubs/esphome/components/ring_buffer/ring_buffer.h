#pragma once
#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>
#include "freertos/FreeRTOS.h"

// Deterministic single-threaded platform adapter. The decoder and zero-copy
// source/transfer buffer implementations are the production files.
namespace esphome::ring_buffer {
class RingBuffer {
 public:
  size_t available() const {
    size_t total = 0;
    for (const auto &chunk : chunks_) total += chunk.size();
    return total;
  }
  size_t write_without_replacement(void *data, size_t length, TickType_t) {
    if (length) {
      auto bytes = static_cast<uint8_t *>(data);
      chunks_.emplace_back(bytes, bytes + length);
    }
    return length;
  }
  void *receive_acquire(size_t &length, size_t limit, TickType_t) {
    assert(held_.empty());
    if (chunks_.empty()) { length = 0; return nullptr; }
    auto &front = chunks_.front();
    length = std::min(front.size(), limit);
    held_.assign(front.begin(), front.begin() + length);
    front.erase(front.begin(), front.begin() + length);
    if (front.empty()) chunks_.pop_front();
    return held_.data();
  }
  void receive_release(void *data) {
    assert(data == held_.data() && !held_.empty());
    held_.clear();
  }
  size_t read(void *data, size_t length, TickType_t ticks) {
    size_t count = 0;
    while (count < length && available()) {
      size_t got = 0;
      void *chunk = receive_acquire(got, length - count, ticks);
      std::memcpy(static_cast<uint8_t *>(data) + count, chunk, got);
      receive_release(chunk); count += got;
    }
    return count;
  }
  void reset() { assert(held_.empty()); chunks_.clear(); }
 private:
  std::deque<std::vector<uint8_t>> chunks_;
  std::vector<uint8_t> held_;
};
}
