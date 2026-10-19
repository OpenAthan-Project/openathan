#pragma once
#include "esphome/core/defines.h"
#if !defined(OPENATHAN_DUA_QUALIFICATION) || !defined(OPENATHAN_PROVISIONING_TEST_STORAGE) || !defined(OPENATHAN_UPDATES_ENABLED) || OPENATHAN_UPDATES_ENABLED
#error "Dua qualification requires isolated storage and disabled firmware updates"
#endif
#include <cstdint>
#include <string>
#include <esp_http_client.h>
#include "esphome/components/json/json_util.h"

namespace esphome::openathan_dua_qualification {
inline constexpr const char *RELEASE_BASE = OPENATHAN_DUA_QUALIFICATION_ORIGIN "/releases/";
bool allowed_url(const std::string &url);
void configure_tls(esp_http_client_config_t &config);
void worker_finished(uint32_t bytes);
void snapshot(JsonObject root);
}
