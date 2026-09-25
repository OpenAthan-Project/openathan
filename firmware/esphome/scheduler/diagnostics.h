#pragma once
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "esphome/core/log.h"
inline void openathan_diagnostics() {
  const auto *partition = esp_ota_get_running_partition();
  esp_ota_img_states_t state{};
  const auto result = partition ? esp_ota_get_state_partition(partition, &state) : ESP_FAIL;
  ESP_LOGI("oa_diagnostics", "slot=%s ota_state=%d reset=%d heap=%u largest=%u psram=%u minimum_heap=%u",
      partition ? partition->label : "unknown", result == ESP_OK ? int(state) : -1, int(esp_reset_reason()),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), unsigned(esp_get_minimum_free_heap_size()));
}
inline void openathan_audio_digest() {
  const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "athan_audio");
  uint8_t digest[32];
  if (!partition || partition->address != 0x410000 || partition->size != 0x380000 ||
      esp_partition_get_sha256(partition, digest) != ESP_OK) {
    ESP_LOGE("oa_diagnostics", "Audio partition digest unavailable");
    return;
  }
  char hex[65];
  for (unsigned i = 0; i < 32; ++i) snprintf(hex + 2*i, 3, "%02x", digest[i]);
  ESP_LOGI("oa_diagnostics", "Audio partition SHA-256=%s", hex);
}
