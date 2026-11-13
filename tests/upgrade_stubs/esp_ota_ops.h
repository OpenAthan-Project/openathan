#pragma once
#include <esp_partition.h>
#include <esp_flash_partitions.h>
#include <bootloader_common.h>
#include <array>
#include <cstring>
#include <string>
#include <cassert>
using esp_ota_handle_t=unsigned;
constexpr int ESP_ERR_NOT_FOUND=0x105, ESP_ERR_OTA_VALIDATE_FAILED=0x1503;
struct esp_app_desc_t {char version[32];};
inline esp_partition_t running{0x200000,0x10000,0x1000,ESP_PARTITION_TYPE_APP,0x10},
    inactive{0x200000,0x210000,0x1000,ESP_PARTITION_TYPE_APP,0x11},
    otadata{0x2000,0xe000,0x1000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA};
inline bool raw_read_fails=false, raw_metadata_override=false;
inline int raw_read_failure_sector=-1;
inline unsigned raw_read_calls{};
inline std::array<esp_ota_select_entry_t,2> raw_metadata;
inline bool metadata_erased=false;
inline esp_ota_img_states_t running_state=ESP_OTA_IMG_VALID;
inline esp_ota_img_states_t inactive_state=ESP_OTA_IMG_UNDEFINED;
inline const esp_partition_t *boot_partition=&running;
inline void (*on_boot_selection)()=nullptr;
inline bool selection_fails=false, selection_writes_before_failure=false;
inline bool boot_query_fails=false, description_query_fails=false, candidate_query_fails=false;
inline int inactive_state_result=ESP_OK;
inline std::string inactive_version="v0.3.0";
inline std::string flashed;
inline unsigned boot_selections{},aborts{},rollbacks{},confirmations{};
inline unsigned erases{},writes{};
inline unsigned live_ota_handles{};
inline bool begin_allocation_fails=false,begin_erase_fails=false;
inline bool image_valid=true;
inline int write_error=ESP_OK;
inline const esp_partition_t *esp_ota_get_running_partition(){return &running;}
inline const esp_partition_t *esp_ota_get_boot_partition(){return boot_query_fails?nullptr:boot_partition;}
inline const esp_partition_t *esp_ota_get_next_update_partition(void *){return candidate_query_fails?nullptr:&inactive;}
inline uint8_t esp_ota_get_app_partition_count(){return 2;}
inline int esp_ota_begin(const esp_partition_t *,unsigned,unsigned *handle){if(begin_allocation_fails)return -1;*handle=++live_ota_handles;++erases;if(begin_erase_fails)return -1;flashed.clear();inactive_state=ESP_OTA_IMG_UNDEFINED;return 0;}
inline int esp_ota_write(unsigned,const void *data,size_t size){++writes;if(write_error!=ESP_OK)return write_error;if(size&&flashed.empty()&&*static_cast<const uint8_t *>(data)!=0xe9)return ESP_ERR_OTA_VALIDATE_FAILED;flashed.append(static_cast<const char *>(data),size);return 0;}
inline int esp_ota_abort(unsigned handle){assert(handle && live_ota_handles);++aborts;--live_ota_handles;return 0;}
inline int esp_ota_end(unsigned handle){assert(handle && live_ota_handles);--live_ota_handles;return image_valid?0:ESP_ERR_OTA_VALIDATE_FAILED;}
inline int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *desc){if(description_query_fails)return -1;strcpy(desc->version,inactive_version.c_str());return 0;}
inline int esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state){*state=partition==&running?running_state:inactive_state;return partition==&running?(metadata_erased?-1:0):inactive_state_result;}
inline int esp_ota_mark_app_valid_cancel_rollback(){++confirmations;running_state=ESP_OTA_IMG_VALID;return 0;}
inline int esp_ota_mark_app_invalid_rollback_and_reboot(){++rollbacks;return 0;}
inline int esp_ota_set_boot_partition(const esp_partition_t *partition){++boot_selections;if(selection_fails&&!selection_writes_before_failure)return -1;boot_partition=partition;if(partition==&running){metadata_erased=false;running_state=ESP_OTA_IMG_NEW;}else inactive_state=ESP_OTA_IMG_NEW;if(on_boot_selection)on_boot_selection();return selection_fails?-1:0;}
