#pragma once
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_psram.h>
#include <esp_flash.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <cstdio>
#include "esphome/components/i2c/i2c.h"
inline std::string waveshare_diagnostics(esphome::i2c::I2CBus &bus, esphome::i2c::I2CBus &alternate,
                                        bool playing, bool dac_failed, bool lcd_failed) {
  uint32_t flash = 0;
  esp_flash_get_size(esp_flash_default_chip, &flash);
  esp_ota_img_states_t state{};
  const auto result = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state);
  const auto probe = [](esphome::i2c::I2CBus &value, uint8_t address) {
    return value.write_readv(address, nullptr, 0, nullptr, 0) == esphome::i2c::ERROR_OK;
  };
  char text[512];
  snprintf(text, sizeof(text), "reset=%d flash=%lu psram_size=%u internal_free=%u internal_largest=%u internal_min=%u psram_free=%u psram_largest=%u playing=%d dac_failed=%d lcd_failed=%d bus10_11_codec=%d bus10_11_expander=%d bus12_13_codec=%d bus12_13_expander=%d ota_state=%d uptime_ms=%lld",
      int(esp_reset_reason()), static_cast<unsigned long>(flash), unsigned(esp_psram_get_size()),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)), playing, dac_failed, lcd_failed,
      probe(bus, 0x18), probe(bus, 0x20), probe(alternate, 0x18), probe(alternate, 0x20),
      result == ESP_OK ? int(state) : -1, static_cast<long long>(esp_timer_get_time()/1000));
  return std::string(text);
}
