#include "qualification.h"
#include <atomic>
#include <cstdint>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace esphome::openathan_quran_qualification {
namespace {
constexpr auto MARKER = "OPENATHAN_QURAN_QUALIFICATION_V1";
std::atomic<unsigned> stack_min{UINT32_MAX}, jobs{0}, requests{0};
}
std::string route(const std::string &logical_url) {
  // QuranHttp has already checked the original provider URL. Preserve its host
  // and path so fixtures can return logical redirects through that same policy.
  ++requests;
  return std::string(OPENATHAN_QURAN_QUALIFICATION_ORIGIN) + "/" + MARKER + "/" + logical_url.substr(8);
}
void configure_tls(esp_http_client_config_t &config) {
  config.crt_bundle_attach = nullptr;
  config.cert_pem = OPENATHAN_QURAN_QUALIFICATION_CA;
  // Hostname verification remains enabled for the private origin's certificate.
}
void worker_finished(unsigned minimum_free_bytes) {
  ++jobs;
  if (minimum_free_bytes < stack_min.load()) stack_min = minimum_free_bytes;
}
void snapshot(JsonObject root) {
  auto result = root["qualification"].to<JsonObject>();
  result["identity"] = MARKER;
  result["origin"] = OPENATHAN_QURAN_QUALIFICATION_ORIGIN;
  result["http_requests"] = requests.load();
  result["metadata_jobs"] = jobs.load();
  if (stack_min != UINT32_MAX) result["metadata_stack_min_free_bytes"] = stack_min.load();
  result["internal_free"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  result["internal_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  result["internal_min"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  result["psram_free"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  result["psram_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  result["reset_reason"] = int(esp_reset_reason());
}
}
