#pragma once
#include "fake_i2s.h"
namespace esp_audio_libs::pcm_convert {
inline void copy_frames(uint8_t*out,uint8_t*in,uint8_t iw,uint8_t ic,uint8_t ow,uint8_t,uint32_t frames){
 for(size_t i=0;i<size_t(frames)*ic;++i)std::memmove(out+i*ow,in+i*iw+iw-ow,ow);
}
}
