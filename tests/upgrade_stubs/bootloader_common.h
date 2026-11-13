#pragma once
#include <esp_flash_partitions.h>
#include <cstddef>
inline uint32_t bootloader_common_ota_select_crc(const esp_ota_select_entry_t *entry) {
  uint32_t crc=0;
  for(unsigned byte=0;byte<4;++byte) {
    crc^=(entry->ota_seq>>(byte*8))&0xff;
    for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);
  }
  return ~crc;
}
