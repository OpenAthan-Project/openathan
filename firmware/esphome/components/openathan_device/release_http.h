#pragma once
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <string>
#include <strings.h>
namespace esphome::openathan_device::release_http {
inline bool allowed_url(const std::string &url) {
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
  return openathan_upgrade_qualification::allowed_url(url);
#else
  // Redirects are necessary for GitHub release storage. Never send device
  // credentials, and never follow an insecure or arbitrary-host redirect.
  for (const char *host : {"https://github.com/", "https://release-assets.githubusercontent.com/",
                           "https://objects.githubusercontent.com/"})
    if (url.rfind(host, 0) == 0 && url.size() <= 4096) return true;
  return false;
#endif
}
struct Response {
  esp_http_client_handle_t client{};
  std::string location;
  int status{};
  static esp_err_t event(esp_http_client_event_t *event) {
    auto *response = static_cast<Response *>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_HEADER && strcasecmp(event->header_key, "Location") == 0)
      response->location = event->header_value;
    return ESP_OK;
  }
  ~Response() { if (client) esp_http_client_cleanup(client); }
  bool open(std::string url) {
    for (unsigned redirects = 0; redirects <= 4; ++redirects) {
      if (!allowed_url(url)) return false;
      esp_http_client_config_t config{};
      config.url = url.c_str();
#ifdef OPENATHAN_UPGRADE_QUALIFICATION
      openathan_upgrade_qualification::configure_tls(config);
#else
      config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
      config.timeout_ms = 4000;
      config.disable_auto_redirect = true;
      config.buffer_size = 4096;
      // IDF formats the request line and the first headers in this buffer.
      // GitHub asset redirects carry long queries; validated URLs cap this at
      // 4608 bytes while short requests allocate only what they need.
      config.buffer_size_tx = static_cast<int>(url.size() + 512);
      config.event_handler = event;
      config.user_data = this;
      location.clear();
      client = esp_http_client_init(&config);
      if (!client || esp_http_client_open(client, 0) != ESP_OK || esp_http_client_fetch_headers(client) < 0) return false;
      status = esp_http_client_get_status_code(client);
      if (status == 200) return true;
      if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308) return false;
      if (location.empty()) return false;
      url = location;
      esp_http_client_cleanup(client);
      client = nullptr;
    }
    return false;
  }
};
}
