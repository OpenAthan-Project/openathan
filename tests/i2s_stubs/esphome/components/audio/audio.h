#pragma once
#include "fake_i2s.h"
namespace esphome::audio {
class AudioStreamInfo {
 public:
 AudioStreamInfo(uint8_t b=16,uint8_t c=1,uint32_t r=44100):bits_(b),channels_(c),rate_(r){}
 uint8_t get_channels()const{return channels_;}uint8_t get_bits_per_sample()const{return bits_;}
 uint32_t get_sample_rate()const{return rate_;}
 size_t samples_to_bytes(size_t n)const{return n*bits_/8;}
 size_t frames_to_bytes(size_t n)const{return samples_to_bytes(n)*channels_;}
 uint32_t bytes_to_frames(size_t n)const{return n/frames_to_bytes(1);}
 uint32_t ms_to_frames(uint32_t ms)const{return ms*rate_/1000;}
 size_t ms_to_bytes(uint32_t ms)const{return frames_to_bytes(ms_to_frames(ms));}
 int64_t frames_to_microseconds(uint32_t n)const{return static_cast<int64_t>(n)*1000000/rate_;}
 bool operator==(const AudioStreamInfo&)const=default;
 private:uint8_t bits_,channels_;uint32_t rate_;
};
}
