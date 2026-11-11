#include "esphome/core/defines.h"
#include "speaker_discovery.h"

#ifdef USE_OPENATHAN_SPEAKER_DISCOVERY

#include <esp_netif.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <mdns.h>

#include <memory>
#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "speaker_finder.h"

namespace esphome::openathan_device {
namespace {
namespace finder = speaker_finder;
constexpr char peer_service[] = "_openathan";
constexpr char alias_service[] = "_oa-finder";
constexpr char tcp[] = "_tcp";
constexpr uint16_t port = 80;
constexpr unsigned query_timeout_ms = 800;
constexpr unsigned shutdown_timeout_ms = 1500;
constexpr char tag[] = "openathan.discovery";
using Results = std::unique_ptr<mdns_result_t, decltype(&mdns_query_results_free)>;

esp_err_t query(const char *host, const char *service, uint16_t type, Results &results) {
  mdns_result_t *raw = nullptr;
  const auto status = mdns_query(host, service, service ? tcp : nullptr, type, query_timeout_ms, 32, &raw);
  results.reset(raw);
  return status;
}

std::string_view txt(const mdns_result_t &result, std::string_view key) {
  if (!result.txt || !result.txt_value_len) return {};
  for (size_t i = 0; i < result.txt_count; ++i) {
    const auto &item = result.txt[i];
    if (item.key && item.value && item.key == key) return {item.value, result.txt_value_len[i]};
  }
  return {};
}

bool discover(const char *service, const esp_netif_t *sta, bool candidates, std::vector<finder::Speaker> &peers) {
  Results results(nullptr, mdns_query_results_free);
  if (query(nullptr, service, MDNS_TYPE_PTR, results) != ESP_OK) return false;
  for (auto *result = results.get(); result; result = result->next) {
    if (result->esp_netif != sta || result->port != port || !result->hostname || !result->ttl) continue;
    if (candidates && txt(*result, "v") != "1") continue;
    finder::record(peers, {result->hostname, candidates ? std::string(txt(*result, "name")) : std::string{},
                          candidates, candidates && txt(*result, "owner") == "1"});
  }
  return true;
}

bool alias_available(const esp_netif_t *sta, const std::atomic<bool> &stopping) {
  if (stopping || mdns_hostname_exists(finder::alias)) return false;
  for (auto type : {MDNS_TYPE_A, MDNS_TYPE_AAAA}) {
    Results results(nullptr, mdns_query_results_free);
    if (stopping || query(finder::alias, nullptr, type, results) != ESP_OK) return false;
    for (auto *result = results.get(); result; result = result->next)
      if (result->esp_netif == sta && result->addr && result->ttl) return false;
  }
  return true;
}

bool claim_alias(const std::string &hostname, const std::string &ip, bool &delegated,
                 const std::atomic<bool> &stopping) {
  in_addr address{};
  if (inet_pton(AF_INET, ip.c_str(), &address) != 1) return false;
  mdns_ip_addr_t addresses{};
  addresses.addr.type = ESP_IPADDR_TYPE_V4;
  addresses.addr.u_addr.ip4.addr = address.s_addr;
  if (stopping || mdns_delegate_hostname_add(finder::alias, &addresses) != ESP_OK ||
      stopping || !mdns_hostname_exists(finder::alias))
    return false;
  delegated = true;
  // The delegate API only inserts an address. An announcement for a service
  // targeting that address also sends a cache-flush A record.
  return !stopping && mdns_service_add_for_host(hostname.c_str(), alias_service, tcp, finder::alias, port, nullptr, 0) == ESP_OK &&
         !stopping && mdns_service_port_set_for_host(hostname.c_str(), alias_service, tcp, finder::alias, port) == ESP_OK;
}
}  // namespace

void SpeakerDiscovery::setup() {
  std::lock_guard<std::mutex> lock(mutex_);
  hostname_ = App.get_name().c_str();
}

void SpeakerDiscovery::on_shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    directory_.clear();
    if (worker_) xTaskNotifyGive(worker_);
  }

  // This component shuts down before mDNS. Wait for the worker to release
  // its query results before ESPHome can free the mDNS service.
  const auto started = xTaskGetTickCount();
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!worker_) return;
    }
    if (xTaskGetTickCount() - started >= pdMS_TO_TICKS(shutdown_timeout_ms)) {
      ESP_LOGE(tag, "Discovery did not stop before mDNS shutdown");
      arch_restart();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void SpeakerDiscovery::update(std::string_view ip, std::string_view name) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || hostname_.empty()) return;
  const bool network_changed = ip_ != ip;
  const bool name_changed = name_ != name;
  if (network_changed) {
    ip_ = ip;
    ++network_generation_;
    directory_.clear();
  }
  if (name_changed) name_ = name;
  if (!worker_ && !ip_.empty()) {
    xTaskCreate([](void *arg) {
      auto *discovery = static_cast<SpeakerDiscovery *>(arg);
      discovery->run_();
      {
        std::lock_guard<std::mutex> lock(discovery->mutex_);
        discovery->worker_ = nullptr;
      }
      vTaskDelete(nullptr);
    }, "oa_discovery", 4608, this, 1, &worker_);
  } else if (worker_ && (network_changed || name_changed)) {
    xTaskNotifyGive(worker_);
  }
}

std::string SpeakerDiscovery::directory(std::string_view ip) {
  std::lock_guard<std::mutex> lock(mutex_);
  return !ip.empty() && ip == ip_ ? directory_ : std::string{};
}

void SpeakerDiscovery::run_() {
  finder::Election election;
  bool owner = false;
  bool delegated = false;
  bool advertised = false;
  bool advertised_owner = false;
  std::string advertised_name;
  unsigned generation = 0;
  while (!stopping_) {
    std::string ip, name;
    unsigned network_generation;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ip = ip_;
      name = name_;
      network_generation = network_generation_;
    }
    if (generation != network_generation) {
      owner = false;
      election = {};
      generation = network_generation;
      if (!stopping_ && advertised && mdns_service_remove(peer_service, tcp) == ESP_OK) advertised = false;
    }
    if (!stopping_ && !owner && delegated && mdns_delegate_hostname_remove(finder::alias) == ESP_OK) delegated = false;
    const auto *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (ip.empty() || !sta) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
      continue;
    }
    if (stopping_) break;
    mdns_txt_item_t metadata[] = {{"v", "1"}, {"name", name.c_str()}, {"owner", owner ? "1" : "0"}};
    if (!advertised) {
      advertised = mdns_service_add(hostname_.c_str(), peer_service, tcp, port, metadata, 3) == ESP_OK;
      advertised_name = name;
      advertised_owner = owner;
    } else if ((name != advertised_name || owner != advertised_owner) &&
               mdns_service_txt_set(peer_service, tcp, metadata, 3) == ESP_OK) {
      advertised_name = name;
      advertised_owner = owner;
    }

    if (stopping_) break;
    std::vector<finder::Speaker> peers;
    peers.reserve(finder::max_speakers);
    finder::record(peers, {hostname_, name, true, owner});
    if (!advertised || !discover(peer_service, sta, true, peers)) {
      election = {};
    } else {
      switch (election.observe(hostname_, owner, peers)) {
        case finder::Action::WAIT: break;
        case finder::Action::YIELD:
          owner = false;
          if (!stopping_ && mdns_delegate_hostname_remove(finder::alias) == ESP_OK) delegated = false;
          break;
        case finder::Action::CHECK_ALIAS: {
          if (delegated || !alias_available(sta, stopping_)) break;
          {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation != network_generation_) break;
          }
          owner = claim_alias(hostname_, ip, delegated, stopping_);
          break;
        }
      }
    }

    if (stopping_) break;
    if (owner) discover("_http", sta, false, peers);
    auto html = owner ? finder::page(peers) : std::string{};
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!stopping_ && generation == network_generation_) directory_ = std::move(html);
    }
    // Reconcile ownership promptly after a transition, and jitter later
    // rounds so neighboring speakers do not repeatedly transmit in sync.
    const unsigned wait_ms = owner != advertised_owner ? 100 : 3500 + esp_random() % 750;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
  }
}
}  // namespace esphome::openathan_device

#endif
