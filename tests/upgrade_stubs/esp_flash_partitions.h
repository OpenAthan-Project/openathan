#pragma once
#include <cstdint>
enum esp_ota_img_states_t : uint32_t {
  ESP_OTA_IMG_NEW=0, ESP_OTA_IMG_PENDING_VERIFY=1, ESP_OTA_IMG_VALID=2,
  ESP_OTA_IMG_INVALID=3, ESP_OTA_IMG_ABORTED=4, ESP_OTA_IMG_UNDEFINED=UINT32_MAX
};
struct esp_ota_select_entry_t {
  uint32_t ota_seq;
  uint8_t seq_label[20];
  uint32_t ota_state;
  uint32_t crc;
};
static_assert(sizeof(esp_ota_select_entry_t)==32);
