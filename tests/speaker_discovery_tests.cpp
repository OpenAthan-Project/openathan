#include "speaker_discovery_platform.h"
#include "speaker_discovery.h"

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using esphome::openathan_device::SpeakerDiscovery;
constexpr char ip[] = "192.168.1.12";
struct Restart {};
struct Platform {
  std::thread worker;
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<uint16_t> queries;
  unsigned sleeps{}, notifications{}, ticks{}, freed{};
  uint16_t blocked_query{};
  bool querying{}, release_query{}, stall{}, delegated{}, exited{}, legacy_peer{};

  void wait_for_sleep(unsigned count) {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return sleeps >= count; });
  }
  void wait_for_query() {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return querying; });
  }
};
Platform *platform;
const esp_netif_t sta;

void single_speaker() {
  Platform state;
  platform = &state;
  SpeakerDiscovery discovery;
  discovery.update(ip, "Kitchen");
  assert(!state.worker.joinable());
  discovery.setup();
  discovery.update(ip, "Kitchen");
  state.wait_for_sleep(1);
  xTaskNotifyGive(&state);
  state.wait_for_sleep(2);
  constexpr std::string_view link = "http://openathan-a1b2c3.local/";
  auto page = discovery.directory(ip);
  auto position = page.find(link);
  assert(position != std::string::npos && page.find(link, position + link.size()) == std::string::npos);
  assert(page.find("<bdi>Kitchen</bdi><small>a1b2c3</small>") != std::string::npos);

  {
    std::lock_guard lock(state.mutex);
    state.legacy_peer = true;
  }
  discovery.update(ip, "");
  state.wait_for_sleep(3);
  page = discovery.directory(ip);
  assert(page.find("<bdi>OpenAthan</bdi><small>a1b2c3</small>") != std::string::npos);
  assert(page.find("http://openathan-abcdef.local/") != std::string::npos);
  assert(page.find("<bdi>OpenAthan</bdi><small>abcdef</small>") != std::string::npos);
  discovery.on_shutdown();
  state.worker.join();
  assert(state.exited && discovery.directory(ip).empty());
  discovery.update(ip, "Kitchen");
  assert(!state.worker.joinable());
}

void stop_during_query(uint16_t type, bool stall) {
  Platform state;
  state.blocked_query = type;
  state.stall = stall;
  platform = &state;
  SpeakerDiscovery discovery;
  discovery.setup();
  discovery.update(ip, "Kitchen");
  if (type == MDNS_TYPE_A) {
    state.wait_for_sleep(1);
    xTaskNotifyGive(&state);
  }
  state.wait_for_query();
  try {
    discovery.on_shutdown();
    assert(!stall && state.freed == 1);
  } catch (const Restart &) {
    assert(stall && state.ticks == 1500 && state.freed == 0);
    std::lock_guard lock(state.mutex);
    state.release_query = true;
    state.changed.notify_all();
  }
  state.worker.join();
  assert(state.exited && state.freed == 1 && !state.delegated);
  const std::vector<uint16_t> expected = type == MDNS_TYPE_A
      ? std::vector<uint16_t>{MDNS_TYPE_PTR, MDNS_TYPE_PTR, MDNS_TYPE_A}
      : std::vector<uint16_t>{MDNS_TYPE_PTR};
  assert(state.queries == expected);
}
}  // namespace

void esphome::arch_restart() { throw Restart{}; }
unsigned xTaskGetTickCount() { return platform->ticks; }
void xTaskCreate(void (*entry)(void *), const char *, unsigned, void *arg, unsigned, TaskHandle_t *task) {
  *task = platform;
  platform->worker = std::thread(entry, arg);
}
void xTaskNotifyGive(TaskHandle_t task) {
  auto &state = *static_cast<Platform *>(task);
  std::lock_guard lock(state.mutex);
  ++state.notifications;
  state.changed.notify_all();
}
void ulTaskNotifyTake(int, unsigned) {
  auto &state = *platform;
  std::unique_lock lock(state.mutex);
  ++state.sleeps;
  state.changed.notify_all();
  state.changed.wait(lock, [&] { return state.notifications != 0; });
  state.notifications = 0;
}
void vTaskDelay(unsigned ticks) {
  auto &state = *platform;
  std::unique_lock lock(state.mutex);
  if (state.stall) {
    state.ticks += ticks;
  } else {
    state.release_query = true;
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return state.exited; });
  }
}
void vTaskDelete(TaskHandle_t) {
  std::lock_guard lock(platform->mutex);
  platform->exited = true;
  platform->changed.notify_all();
}

const esp_netif_t *esp_netif_get_handle_from_ifkey(const char *) { return &sta; }
unsigned esp_random() { return 0; }
esp_err_t mdns_query(const char *, const char *service, const char *, uint16_t type, unsigned, unsigned,
                     mdns_result_t **result) {
  auto &state = *platform;
  std::unique_lock lock(state.mutex);
  state.queries.push_back(type);
  if (type == state.blocked_query) {
    state.querying = true;
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return state.release_query; });
    *result = new mdns_result_t{};
  } else if (state.legacy_peer && service && std::string_view(service) == "_http") {
    *result = new mdns_result_t{.esp_netif = &sta, .port = 80, .hostname = "openathan-abcdef", .ttl = 120};
  }
  return ESP_OK;
}
void mdns_query_results_free(mdns_result_t *result) {
  delete result;
  ++platform->freed;
}
bool mdns_hostname_exists(const char *) { return platform->delegated; }
esp_err_t mdns_delegate_hostname_add(const char *, const mdns_ip_addr_t *) {
  platform->delegated = true;
  return ESP_OK;
}
esp_err_t mdns_delegate_hostname_remove(const char *) { return ESP_OK; }
esp_err_t mdns_service_add_for_host(const char *instance, const char *, const char *, const char *,
                                   uint16_t, const mdns_txt_item_t *, unsigned) {
  assert(std::string_view(instance) == esphome::App.get_name());
  return ESP_OK;
}
esp_err_t mdns_service_port_set_for_host(const char *, const char *, const char *, const char *, uint16_t) {
  return ESP_OK;
}
esp_err_t mdns_service_add(const char *instance, const char *, const char *, uint16_t, const mdns_txt_item_t *, unsigned) {
  assert(std::string_view(instance) == esphome::App.get_name());
  return ESP_OK;
}
esp_err_t mdns_service_remove(const char *, const char *) { return ESP_OK; }
esp_err_t mdns_service_txt_set(const char *, const char *, const mdns_txt_item_t *, unsigned) { return ESP_OK; }

int main() {
  single_speaker();
  stop_during_query(MDNS_TYPE_PTR, false);
  stop_during_query(MDNS_TYPE_A, false);
  stop_during_query(MDNS_TYPE_PTR, true);
}
