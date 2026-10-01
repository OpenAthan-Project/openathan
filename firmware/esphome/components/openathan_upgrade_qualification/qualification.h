#pragma once
#include "esphome/core/defines.h"
#if !defined(OPENATHAN_UPGRADE_QUALIFICATION) || !defined(OPENATHAN_PROVISIONING_TEST_STORAGE)
#error "Qualification code requires an explicit isolated qualification build"
#endif

#include <atomic>
#include <cstdint>
#include <string>
#include <esp_http_client.h>
#include "esphome/components/json/json_util.h"

namespace esphome::openathan_device { class Upgrade; }
namespace esphome::openathan_upgrade_qualification {
inline constexpr const char *RELEASE_BASE = OPENATHAN_QUALIFICATION_ORIGIN "/releases/";
bool allowed_url(const std::string &url);
void configure_tls(esp_http_client_config_t &config);

// Compiled only for the maintainer configuration. The product updater remains
// the engine under test; this adapter owns all fault injection and observation.
class Qualification {
 public:
  bool services_ready(bool confirmed, const std::string &action) const;
  bool startup_health(bool health) const;
  bool prepare_baseline(openathan_device::Upgrade &upgrade);
  void snapshot(JsonObject root);
  void worker_finished(uint32_t free_bytes);
  int action(openathan_device::Upgrade &upgrade, JsonObjectConst input, std::string &error);
  void cancelled();
  void started(bool install) { phase_ = install ? "downloading" : "checking"; }
  void erase_attempt() { ++erase_attempts_; }
  void write_attempt() { ++write_attempts_; }
  void finalize_attempt() { ++finalize_attempts_; }
  bool selected_loop(bool safe);
  bool should_reconcile(const openathan_device::Upgrade &upgrade) const;
  bool persist_handoff(openathan_device::Upgrade &upgrade);
  bool before_selection();
  bool after_selection();
 private:
  bool hold_(const char *phase);
  std::string phase_{"idle"}, arm_;
  bool released_{}, selected_{};
  std::atomic<uint32_t> worker_stack_free_min_{UINT32_MAX};
  std::atomic<uint32_t> erase_attempts_{0}, write_attempts_{0}, finalize_attempts_{0}, selection_attempts_{0};
};
}  // namespace esphome::openathan_upgrade_qualification
