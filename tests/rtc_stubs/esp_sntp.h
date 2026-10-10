#pragma once
#include <cstddef>
#include <sys/time.h>

enum sntp_sync_status_t { SNTP_SYNC_STATUS_RESET, SNTP_SYNC_STATUS_COMPLETED, SNTP_SYNC_STATUS_IN_PROGRESS };
namespace sntp_test {
inline sntp_sync_status_t status = SNTP_SYNC_STATUS_RESET;
inline void (*notification)(timeval *){};
inline bool enabled{};
inline void reset() { status = SNTP_SYNC_STATUS_RESET; notification = nullptr; enabled = false; }
}
inline sntp_sync_status_t esp_sntp_get_sync_status() {
  const auto result = sntp_test::status;
  if (result == SNTP_SYNC_STATUS_COMPLETED) sntp_test::status = SNTP_SYNC_STATUS_RESET;
  return result;
}
constexpr int ESP_SNTP_OPMODE_POLL = 0;
inline bool esp_sntp_enabled() { return sntp_test::enabled; }
inline void esp_sntp_stop() { sntp_test::enabled = false; }
inline void esp_sntp_setoperatingmode(int) {}
inline void esp_sntp_setservername(size_t, const char *) {}
inline void esp_sntp_set_sync_interval(unsigned) {}
inline void esp_sntp_set_time_sync_notification_cb(void (*callback)(timeval *)) { sntp_test::notification = callback; }
inline void esp_sntp_init() { sntp_test::enabled = true; }
