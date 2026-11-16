#pragma once
#include "athan_api.h"
#include "esphome/components/openathan/openathan.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <nvs.h>

namespace esphome::openathan_device {
class AthanMedia : public AthanApi {
 public:
  void begin(openathan_component::OpenAthan *athan);
  void loop(bool connected, bool permitted, bool firmware_idle, bool firmware_confirmed, bool dua_ready);
  void shutdown() { cancel_ = true; network_safe_ = false; }
  bool active() const { return active_; }
  bool owns_storage() const { return destructive_; }
  void snapshot(JsonObject root) override;
  int action(const std::string &action, JsonObjectConst input, std::string &error) override;

 private:
  enum class Phase : uint32_t { IDLE, QUEUED, WRITING, REBOOT };
  struct SavedTrack {
    char id[49]{}, name[81]{};
    uint16_t reserved{};
    uint32_t bytes{};
    std::array<uint8_t, 32> sha256{};
  };
  struct Record {
    char magic[8]{'O','A','A','T','H','0','0','1'};
    uint32_t revision{};
    Phase phase{Phase::IDLE};
    char version[34]{};
    uint16_t reserved{};
    std::array<uint8_t, 32> catalog{}, firmware{};
    std::array<SavedTrack, 2> installed{}, requested{};
    std::array<uint8_t, 32> checksum{};
  };
  static_assert(sizeof(Record) <= 1024);
  struct Recording {
    SavedTrack saved;
    unsigned role{};
    std::string file, attribution, source, license;
  };
  struct Catalog {
    std::string version;
    std::array<uint8_t, 32> hash{};
    std::vector<Recording> recordings;
  };
  enum class Job { CATALOG, DOWNLOAD, WRITE };
  using Buffer = std::unique_ptr<uint8_t, decltype(&heap_caps_free)>;
  static void worker_(void *argument);
  void run_();
  bool start_(Job job);
  bool save_(Record next);
  bool load_();
  bool scan_(std::array<SavedTrack, 2> &tracks);
  bool fetch_catalog_(const std::string &url, Catalog &out);
  bool verify_catalog_(const std::string &envelope, Catalog &out);
  bool download_(const std::string &url, const SavedTrack &track, uint8_t *out);
  bool write_();
  void fail_(const char *message);
  void quiesce_();
  bool network_allowed_() const { return network_safe_ && !cancel_; }
  bool can_retry_() const;
  openathan_component::OpenAthan *athan_{};
  const esp_partition_t *partition_{};
  nvs_handle_t nvs_{};
  Record record_;
  Catalog catalog_;
  std::array<SavedTrack, 2> installed_{};
  std::array<uint8_t, 32> firmware_{};
  Buffer staged_{nullptr, &heap_caps_free};
  std::mutex mutex_;
  std::atomic<bool> active_{}, cancel_{}, network_safe_{}, destructive_{};
  Job job_{Job::CATALOG};
  bool supported_{}, storage_ok_{}, identity_ok_{}, refresh_{}, staged_ready_{}, boot_verified_{}, quiesced_{}, latched_{}, write_finished_{}, reboot_requested_{};
  std::string state_{"unavailable"}, error_, catalog_state_{"idle"}, catalog_error_;
  uint32_t received_{}, total_{};
  uint64_t started_ms_{};
};
}
