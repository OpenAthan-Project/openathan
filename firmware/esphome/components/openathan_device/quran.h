#pragma once
#include "quran_api.h"
#include "esphome/components/openathan/openathan.h"
#include <atomic>
#include <memory>

namespace esphome::openathan_device {
class Quran : public QuranApi {
 public:
  void begin(openathan_component::OpenAthan *athan) { athan_ = athan; }
  void loop(bool connected, bool blocked);
  void shutdown();
  void snapshot(JsonObject root) override;
  void catalog_snapshot(JsonObject root) override;
  int action(const std::string &action, JsonObjectConst input, std::string &error) override;
 private:
  struct Job {
    std::string kind, response, url, error, query;
    unsigned offset{}, reciter{}, edition{}, surah{};
    uint32_t epoch{}, request{};
    std::atomic<bool> done{}, cancel{};
  };
  static void worker_(void *context);
  void fetch_(Job &job);
  openathan_component::OpenAthan *athan_{};
  std::shared_ptr<Job> job_;
  std::string catalog_state_{"idle"}, catalog_response_, catalog_error_, playback_error_;
  uint32_t request_{}, catalog_request_{};
  unsigned reciter_{}, edition_{}, surah_{};
  bool connected_{}, blocked_{}, resolving_{};
};
}
