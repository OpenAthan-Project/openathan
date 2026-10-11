#pragma once
#include "esphome/core/defines.h"
#if !defined(OPENATHAN_QURAN_QUALIFICATION) || !defined(OPENATHAN_PROVISIONING_TEST_STORAGE) || \
    !defined(OPENATHAN_QURAN) || !defined(OPENATHAN_UPDATES_ENABLED) || OPENATHAN_UPDATES_ENABLED
#error "Quran qualification requires isolated Quran firmware with updates disabled"
#endif
#include <string>
#include <esp_http_client.h>
#include "esphome/components/json/json_util.h"

namespace esphome::openathan_quran_qualification {
std::string route(const std::string &logical_url);
void configure_tls(esp_http_client_config_t &config);
void worker_finished(unsigned minimum_free_bytes);
void snapshot(JsonObject root);
}
