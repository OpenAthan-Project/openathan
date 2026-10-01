#pragma once
#include <cstddef>
#include <cstring>
inline void *esp_flash_default_chip=nullptr;
inline bool boot_readable=true;
inline int esp_flash_read(void *,void *out,unsigned,size_t size){memset(out,0,size);return boot_readable?0:-1;}
