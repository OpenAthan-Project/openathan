#pragma once
#include "esphome/components/ring_buffer/ring_buffer.h"
namespace esphome::audio {
class RingBufferAudioSource {
 public:
 static std::unique_ptr<RingBufferAudioSource>create(std::shared_ptr<ring_buffer::RingBuffer>r,size_t n,uint8_t){
  if(!r||fake_i2s::source_allocation_failure)return {};return std::unique_ptr<RingBufferAudioSource>(new RingBufferAudioSource(r,n));
 }
 size_t fill(TickType_t,bool){
  if(fake_i2s::on_fill)fake_i2s::on_fill();
  size_t n=std::min(size_-data_.size(),ring_->bytes.size());
  for(size_t i=0;i<n;++i){data_.push_back(ring_->bytes.front());ring_->bytes.pop_front();}return n;
 }
 size_t available()const{return data_.size();}uint8_t*mutable_data(){return data_.data();}
 void consume(size_t n){data_.erase(data_.begin(),data_.begin()+n);}
 private:RingBufferAudioSource(std::shared_ptr<ring_buffer::RingBuffer>r,size_t n):ring_(r),size_(n){}
 std::shared_ptr<ring_buffer::RingBuffer>ring_;size_t size_;std::vector<uint8_t>data_;
};
}
