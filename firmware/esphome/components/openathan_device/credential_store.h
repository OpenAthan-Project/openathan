#pragma once
#include <nvs.h>

#include <array>

#include "openathan/settings.h"

namespace esphome::openathan_device {
struct WifiCredentials {
  std::string ssid, password;
};
struct PasswordVerifier {
  uint32_t revision{};
  std::string ha1;
};
class CredentialStore {
 public:
  ~CredentialStore() {
    if (opened_) nvs_close(handle_);
  }
  ::openathan::LoadResult load_wifi(WifiCredentials& value);
  ::openathan::LoadResult load_password(PasswordVerifier& value);
  bool save_wifi(const WifiCredentials& value);
  bool save_password(const PasswordVerifier& value);
  bool healthy() const { return !fault_; }

 private:
  ::openathan::LoadResult read_(const char* key, uint8_t* bytes, size_t length);
  bool write_(const char* key, uint8_t* bytes, size_t length);
  bool open_();
  nvs_handle_t handle_{};
  bool opened_{}, fault_{};
};
}  // namespace esphome::openathan_device
