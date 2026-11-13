#pragma once
#include <nvs.h>
using esp_err_t=int;
using esp_partition_mmap_handle_t=unsigned;
struct esp_partition_t{size_t size;unsigned address;size_t erase_size{0x1000};int type{};int subtype{0x10};};

constexpr int ESP_PARTITION_TYPE_APP=0, ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_OTA=0,
    ESP_PARTITION_SUBTYPE_APP_OTA_MIN=0x10, ESP_PARTITION_SUBTYPE_APP_OTA_MAX=0x20,
    ESP_PARTITION_SUBTYPE_ANY=255, ESP_PARTITION_MMAP_DATA=0;
const esp_partition_t *esp_partition_find_first(int,int,const char *);
int esp_partition_mmap(const esp_partition_t *,size_t,size_t,int,const void **,esp_partition_mmap_handle_t *);
int esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
int esp_partition_write(const esp_partition_t *,size_t,const void *,size_t);
int esp_partition_erase_range(const esp_partition_t *,size_t,size_t);
