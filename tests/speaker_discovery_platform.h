#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <string>

namespace esphome {
namespace setup_priority {
inline constexpr float LATE = -100.0f;
}
struct Component {
  virtual float get_setup_priority() const = 0;
  virtual void setup() = 0;
  virtual void on_shutdown() = 0;
};
struct Application {
  std::string get_name() const { return "openathan-a1b2c3"; }
};
inline Application App;
[[noreturn]] void arch_restart();
}  // namespace esphome

#define ESP_LOGE(tag, ...) (void)(tag)
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
using TaskHandle_t = void *;
unsigned xTaskGetTickCount();
void xTaskCreate(void (*entry)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
void xTaskNotifyGive(TaskHandle_t);
void ulTaskNotifyTake(int, unsigned);
void vTaskDelay(unsigned);
void vTaskDelete(TaskHandle_t);

using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;
inline constexpr int ESP_IPADDR_TYPE_V4 = 0;
inline constexpr int MDNS_TYPE_A = 1, MDNS_TYPE_PTR = 12, MDNS_TYPE_AAAA = 28;
struct esp_netif_t {};
struct mdns_ip_addr_t {
  struct {
    int type;
    union {
      struct { uint32_t addr; } ip4;
    } u_addr;
  } addr;
};
struct mdns_txt_item_t {
  const char *key;
  const char *value;
};
struct mdns_result_t {
  mdns_result_t *next{};
  const esp_netif_t *esp_netif{};
  uint16_t port{};
  const char *hostname{};
  unsigned ttl{};
  mdns_txt_item_t *txt{};
  unsigned txt_count{};
  unsigned *txt_value_len{};
  mdns_ip_addr_t *addr{};
};
const esp_netif_t *esp_netif_get_handle_from_ifkey(const char *);
unsigned esp_random();
esp_err_t mdns_query(const char *, const char *, const char *, uint16_t, unsigned, unsigned, mdns_result_t **);
void mdns_query_results_free(mdns_result_t *);
bool mdns_hostname_exists(const char *);
esp_err_t mdns_delegate_hostname_add(const char *, const mdns_ip_addr_t *);
esp_err_t mdns_delegate_hostname_remove(const char *);
esp_err_t mdns_service_add_for_host(const char *, const char *, const char *, const char *, uint16_t,
                                   const mdns_txt_item_t *, unsigned);
esp_err_t mdns_service_port_set_for_host(const char *, const char *, const char *, const char *, uint16_t);
esp_err_t mdns_service_add(const char *, const char *, const char *, uint16_t, const mdns_txt_item_t *, unsigned);
esp_err_t mdns_service_remove(const char *, const char *);
esp_err_t mdns_service_txt_set(const char *, const char *, const mdns_txt_item_t *, unsigned);
