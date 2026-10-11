#pragma once
#include "quran_policy.h"
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <cstring>

namespace esphome::openathan_device {
// Shared by metadata and the pinned audio reader. Redirect validation happens
// before initializing the next HTTP client, not after following the redirect.
class QuranHttp {
 public:
  esp_http_client_handle_t client{};
  std::string content_type;
  ~QuranHttp() { if (client) esp_http_client_cleanup(client); }
  QuranHttp() = default;
  QuranHttp(const QuranHttp &) = delete;
  QuranHttp &operator=(const QuranHttp &) = delete;
  bool open(std::string url, bool audio = false, int buffer_size = 2048, int timeout_ms = 4000) {
    for (unsigned redirects = 0; redirects <= 4; ++redirects) {
      const bool metadata = url.size() <= 256 && url.rfind("https://www.mp3quran.net/api/v3/", 0) == 0;
      if (audio ? !quran_audio_url(url) : !metadata) return false;
      esp_http_client_config_t config{};
      config.url = url.c_str(); config.crt_bundle_attach = esp_crt_bundle_attach;
      config.disable_auto_redirect = true; config.timeout_ms = timeout_ms;
      config.buffer_size = buffer_size; config.buffer_size_tx = 1024;
      config.event_handler = event_; config.user_data = this;
      location_.clear(); content_type.clear(); client = esp_http_client_init(&config);
      if (!client || esp_http_client_open(client, 0) != 0 || esp_http_client_fetch_headers(client) < 0) return false;
      const int status = esp_http_client_get_status_code(client);
      if (status == 200) return true;
      if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308) return false;
      url = location_; esp_http_client_cleanup(client); client = nullptr;
    }
    return false;
  }
  esp_http_client_handle_t release() {
    // IDF may deliver events while reading; revoke the soon-to-expire callback context.
    esp_http_client_set_user_data(client, nullptr);
    auto result = client; client = nullptr; return result;
  }
 private:
  static esp_err_t event_(esp_http_client_event_t *event) {
    if (!event->user_data || event->event_id != HTTP_EVENT_ON_HEADER) return 0;
    auto *self = static_cast<QuranHttp *>(event->user_data);
    if (strcasecmp(event->header_key, "Location") == 0) {
      self->location_ = strnlen(event->header_value, 513) <= 512 ? event->header_value : "";
    } else if (strcasecmp(event->header_key, "Content-Type") == 0) {
      self->content_type = strnlen(event->header_value, 129) <= 128 ? event->header_value : "";
    }
    return 0;
  }
  std::string location_;
};
}
