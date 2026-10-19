#include "qualification.h"
#include <atomic>
#include <limits>
#include <esp_heap_caps.h>

namespace esphome::openathan_dua_qualification {
namespace { std::atomic<uint32_t> stack_min{std::numeric_limits<uint32_t>::max()}; }
bool allowed_url(const std::string &url) {
  return url.size() <= 4096 && url.rfind(OPENATHAN_DUA_QUALIFICATION_ORIGIN "/", 0) == 0;
}
void configure_tls(esp_http_client_config_t &config) { config.cert_pem = OPENATHAN_DUA_QUALIFICATION_CA; }
void worker_finished(uint32_t bytes) { if (bytes < stack_min.load()) stack_min = bytes; }
void snapshot(JsonObject root) {
  auto q = root["qualification"].to<JsonObject>();
  q["identity"] = "OPENATHAN_DUA_QUALIFICATION_V1";
  q["free_internal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["largest_internal"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["min_internal"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  q["free_psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  q["largest_psram"] = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const auto bytes = stack_min.load();
  if (bytes != std::numeric_limits<uint32_t>::max()) q["worker_stack_min_free"] = bytes;
}
}
