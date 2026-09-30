#pragma once
#include <esp_partition.h>
#include <cstring>
#include <string>
using esp_ota_handle_t=unsigned;
enum esp_ota_img_states_t {ESP_OTA_IMG_VALID,ESP_OTA_IMG_PENDING_VERIFY};
struct esp_app_desc_t {char version[32];};
inline esp_partition_t running{0x200000},inactive{0x200000};
inline esp_ota_img_states_t running_state=ESP_OTA_IMG_VALID;
inline std::string flashed;
inline unsigned boot_selections{},aborts{},rollbacks{},confirmations{};
inline unsigned erases{},writes{};
inline bool image_valid=true;
inline const esp_partition_t *esp_ota_get_running_partition(){return &running;}
inline const esp_partition_t *esp_ota_get_next_update_partition(void *){return &inactive;}
inline int esp_ota_begin(const esp_partition_t *,unsigned,unsigned *handle){++erases;*handle=1;flashed.clear();return 0;}
inline int esp_ota_write(unsigned,const void *data,size_t size){++writes;flashed.append(static_cast<const char *>(data),size);return 0;}
inline int esp_ota_abort(unsigned){++aborts;return 0;}
inline int esp_ota_end(unsigned){return image_valid?0:-1;}
inline int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *desc){strcpy(desc->version,"v0.3.0");return 0;}
inline int esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *state){*state=running_state;return 0;}
inline int esp_ota_mark_app_valid_cancel_rollback(){++confirmations;running_state=ESP_OTA_IMG_VALID;return 0;}
inline int esp_ota_mark_app_invalid_rollback_and_reboot(){++rollbacks;return 0;}
inline int esp_ota_set_boot_partition(const esp_partition_t *){++boot_selections;return 0;}
