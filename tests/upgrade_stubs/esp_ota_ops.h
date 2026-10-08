#pragma once
#include <esp_partition.h>
#include <cstring>
#include <string>
#include <cassert>
using esp_ota_handle_t=unsigned;
enum esp_ota_img_states_t {ESP_OTA_IMG_UNDEFINED,ESP_OTA_IMG_NEW,ESP_OTA_IMG_PENDING_VERIFY,
  ESP_OTA_IMG_VALID,ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED};
struct esp_app_desc_t {char version[32];};
inline esp_partition_t running{0x200000,0x10000},inactive{0x200000,0x210000};
inline bool metadata_erased=false;
inline esp_ota_img_states_t running_state=ESP_OTA_IMG_VALID;
inline esp_ota_img_states_t inactive_state=ESP_OTA_IMG_UNDEFINED;
inline const esp_partition_t *boot_partition=&running;
inline void (*on_boot_selection)()=nullptr;
inline bool selection_fails=false, selection_writes_before_failure=false;
inline std::string inactive_version="v0.3.0";
inline std::string flashed;
inline unsigned boot_selections{},aborts{},rollbacks{},confirmations{};
inline unsigned erases{},writes{};
inline unsigned live_ota_handles{};
inline bool begin_allocation_fails=false,begin_erase_fails=false;
inline bool image_valid=true;
inline const esp_partition_t *esp_ota_get_running_partition(){return &running;}
inline const esp_partition_t *esp_ota_get_boot_partition(){return boot_partition;}
inline const esp_partition_t *esp_ota_get_next_update_partition(void *){return &inactive;}
inline int esp_ota_begin(const esp_partition_t *,unsigned,unsigned *handle){if(begin_allocation_fails)return -1;*handle=++live_ota_handles;++erases;if(begin_erase_fails)return -1;flashed.clear();inactive_state=ESP_OTA_IMG_UNDEFINED;return 0;}
inline int esp_ota_write(unsigned,const void *data,size_t size){++writes;flashed.append(static_cast<const char *>(data),size);return 0;}
inline int esp_ota_abort(unsigned handle){assert(handle && live_ota_handles);++aborts;--live_ota_handles;return 0;}
inline int esp_ota_end(unsigned handle){assert(handle && live_ota_handles);--live_ota_handles;return image_valid?0:-1;}
inline int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *desc){strcpy(desc->version,inactive_version.c_str());return 0;}
inline int esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state){*state=partition==&running?running_state:inactive_state;return partition==&running&&metadata_erased?-1:0;}
inline int esp_ota_mark_app_valid_cancel_rollback(){++confirmations;running_state=ESP_OTA_IMG_VALID;return 0;}
inline int esp_ota_mark_app_invalid_rollback_and_reboot(){++rollbacks;return 0;}
inline int esp_ota_set_boot_partition(const esp_partition_t *partition){++boot_selections;if(selection_fails&&!selection_writes_before_failure)return -1;boot_partition=partition;if(partition==&running){metadata_erased=false;running_state=ESP_OTA_IMG_NEW;}else inactive_state=ESP_OTA_IMG_NEW;if(on_boot_selection)on_boot_selection();return selection_fails?-1:0;}
