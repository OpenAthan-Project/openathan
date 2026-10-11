#pragma once
#include "fake_i2s.h"
namespace esphome::ring_buffer {
class RingBuffer {
 public:std::deque<uint8_t> bytes;
 static std::shared_ptr<RingBuffer>create(size_t){return fake_i2s::allocation_failure||fake_i2s::ring_allocation_failure?nullptr:std::make_shared<RingBuffer>();}
 size_t available()const{return bytes.size();}
 size_t write_without_replacement(void*d,size_t n,TickType_t){auto p=static_cast<uint8_t*>(d);bytes.insert(bytes.end(),p,p+n);return n;}
};
}
