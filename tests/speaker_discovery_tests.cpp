#include "speaker_discovery_platform.h"
#include "speaker_discovery.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <limits>
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
  std::vector<TickType_t> refreshes;
  std::atomic<TickType_t> ticks{};
  unsigned sleeps{}, notifications{}, freed{}, withdrawals{};
  uint16_t blocked_query{};
  bool querying{}, release_query{}, stall{}, delegated{}, exited{}, legacy_peer{};
  bool other_owner{}, defer_withdrawal{}, block_refresh{}, fail_first_refresh{};

  void wait_for_sleep(unsigned count) {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return sleeps >= count; });
  }
  void wait_for_query() {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return querying; });
  }
  void next_round(TickType_t elapsed = 0) {
    std::unique_lock lock(mutex);
    const auto next_sleep = sleeps + 1;
    ticks += elapsed;
    ++notifications;
    changed.notify_all();
    changed.wait(lock, [&] { return sleeps >= next_sleep; });
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

void reunion_preserves_cached_clients(TickType_t started, bool fail_first_refresh) {
  Platform state;
  state.ticks = started;
  state.fail_first_refresh = fail_first_refresh;
  platform = &state;
  SpeakerDiscovery discovery;
  discovery.setup();
  discovery.update(ip, "Kitchen");
  state.wait_for_sleep(1);
  state.next_round();
  assert(!discovery.directory(ip).empty());
  {
    std::lock_guard lock(state.mutex);
    state.other_owner = true;
    state.defer_withdrawal = true;
  }
  state.next_round();
  if (discovery.directory(ip).empty()) {
    std::fputs("Cached finder client lost its directory as soon as the speaker yielded\n", stderr);
    std::abort();
  }
  state.next_round(2000);
  assert(state.withdrawals == 1 && state.delegated && state.refreshes.empty());
  {
    std::lock_guard lock(state.mutex);
    state.delegated = false;
  }
  state.next_round();
  const auto withdrawn_at = state.ticks.load();
  assert(discovery.directory(ip).find("http://openathan-000001.local/") != std::string::npos);
  state.next_round(1099);
  assert(state.refreshes.empty());
  state.next_round(1);
  state.next_round(3500);
  state.next_round(3500);
  assert((state.refreshes == std::vector<TickType_t>{withdrawn_at + 1100, withdrawn_at + 4600}));
  state.ticks = withdrawn_at + 124999;
  assert(!discovery.directory(ip).empty());
  state.ticks = withdrawn_at + 125000;
  assert(discovery.directory(ip).empty());
  discovery.on_shutdown();
  state.worker.join();
}

enum class CancelHandoff { NETWORK, ADDRESS, REACQUIRE, SHUTDOWN };
void cancel_handoff(CancelHandoff reason) {
  Platform state;
  platform = &state;
  SpeakerDiscovery discovery;
  discovery.setup();
  discovery.update(ip, "Kitchen");
  state.wait_for_sleep(1);
  state.next_round();
  {
    std::lock_guard lock(state.mutex);
    state.other_owner = true;
  }
  state.next_round();
  state.next_round();
  assert(!state.delegated && !discovery.directory(ip).empty() && state.refreshes.empty());
  if (reason == CancelHandoff::NETWORK || reason == CancelHandoff::ADDRESS) {
    discovery.update(reason == CancelHandoff::ADDRESS ? "192.168.1.13" : "", "Kitchen");
    assert(discovery.directory(ip).empty());
    state.wait_for_sleep(5);
    state.next_round(2000);
    assert(discovery.directory(ip).empty());
  } else if (reason == CancelHandoff::REACQUIRE) {
    {
      std::lock_guard lock(state.mutex);
      state.other_owner = false;
    }
    state.next_round();
    state.next_round();
    assert(state.delegated && !discovery.directory(ip).empty());
    state.next_round(125000);
    assert(!discovery.directory(ip).empty());
  }
  discovery.on_shutdown();
  state.worker.join();
  assert(state.refreshes.empty() && discovery.directory(ip).empty());
}

void stop_during_refresh(bool stall) {
  Platform state;
  state.stall = stall;
  platform = &state;
  SpeakerDiscovery discovery;
  discovery.setup();
  discovery.update(ip, "Kitchen");
  state.wait_for_sleep(1);
  state.next_round();
  {
    std::lock_guard lock(state.mutex);
    state.other_owner = true;
    state.block_refresh = true;
  }
  state.next_round();
  state.next_round();
  state.ticks = 1100;
  xTaskNotifyGive(&state);
  state.wait_for_query();
  const auto queries_before = state.queries.size();
  const auto freed_before = state.freed;
  try {
    discovery.on_shutdown();
    assert(!stall && state.freed == freed_before + 1);
  } catch (const Restart &) {
    assert(stall && state.freed == freed_before);
    std::lock_guard lock(state.mutex);
    state.release_query = true;
    state.changed.notify_all();
  }
  state.worker.join();
  assert(state.exited && state.freed == freed_before + 1 && state.queries.size() == queries_before);
  assert(state.refreshes.size() == 1 && discovery.directory(ip).empty());
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
TickType_t xTaskGetTickCount() { return platform->ticks; }
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
  } else if (state.other_owner && service && std::string_view(service) == "_openathan") {
    static mdns_txt_item_t records[] = {{"v", "1"}, {"owner", "1"}};
    static unsigned lengths[] = {1, 1};
    *result = new mdns_result_t{.esp_netif = &sta, .port = 80, .hostname = "openathan-000001", .ttl = 120,
                               .txt = records, .txt_count = 2, .txt_value_len = lengths};
  } else if (state.legacy_peer && service && std::string_view(service) == "_http") {
    *result = new mdns_result_t{.esp_netif = &sta, .port = 80, .hostname = "openathan-abcdef", .ttl = 120};
  }
  return ESP_OK;
}
esp_err_t mdns_query_generic(const char *host, const char *service, const char *proto, uint16_t type,
                             mdns_query_transmission_type_t transmission, unsigned, size_t, mdns_result_t **result) {
  auto &state = *platform;
  std::unique_lock lock(state.mutex);
  assert(std::string_view(host) == "openathan" && !service && !proto);
  assert(type == MDNS_TYPE_A && transmission == MDNS_QUERY_MULTICAST && !state.delegated);
  state.refreshes.push_back(xTaskGetTickCount());
  if (state.block_refresh) {
    state.querying = true;
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return state.release_query; });
    *result = new mdns_result_t{};
  } else {
    *result = nullptr;
  }
  return state.fail_first_refresh && state.refreshes.size() == 1 ? -1 : ESP_OK;
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
esp_err_t mdns_delegate_hostname_remove(const char *) {
  ++platform->withdrawals;
  if (!platform->defer_withdrawal) platform->delegated = false;
  return ESP_OK;
}
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
  reunion_preserves_cached_clients(0, false);
  reunion_preserves_cached_clients(std::numeric_limits<TickType_t>::max() - 2000, true);
  cancel_handoff(CancelHandoff::NETWORK);
  cancel_handoff(CancelHandoff::ADDRESS);
  cancel_handoff(CancelHandoff::REACQUIRE);
  cancel_handoff(CancelHandoff::SHUTDOWN);
  stop_during_refresh(false);
  stop_during_refresh(true);
  stop_during_query(MDNS_TYPE_PTR, false);
  stop_during_query(MDNS_TYPE_A, false);
  stop_during_query(MDNS_TYPE_PTR, true);
}
