#pragma once
#include "fake_i2s.h"
namespace esp_audio_libs::gain {
inline int32_t db_to_q31(float db){return static_cast<int32_t>(std::pow(10.0,db/20.0)*INT32_MAX);}
inline void apply(uint8_t*out,uint8_t*in,int32_t factor,uint32_t n,size_t width){
 for(uint32_t i=0;i<n;++i){int32_t sample=0;
  std::memcpy(&sample,in+i*width,width);
  if(width<4)sample=static_cast<int32_t>(static_cast<uint32_t>(sample)<<(32-width*8))>>(32-width*8);
  const int32_t scaled=static_cast<int64_t>(sample)*factor/(int64_t(1)<<31);
  std::memcpy(out+i*width,&scaled,width);
 }
}
}
