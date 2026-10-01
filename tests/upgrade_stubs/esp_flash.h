#pragma once
#include <cstddef>
#include <cstring>
inline void *esp_flash_default_chip=nullptr;
inline bool boot_readable=true;
inline bool metadata_corrupt=false;
inline int esp_flash_read(void *,void *out,unsigned offset,size_t size){memset(out,offset>=0xe000&&!metadata_corrupt?0xff:0,size);return boot_readable?0:-1;}
