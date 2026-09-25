#include "credential_store.h"

#include <algorithm>
#include <cstring>

#include "protocol.h"

namespace esphome::openathan_device {
namespace {
uint32_t crc(const uint8_t* data, size_t size) {
  uint32_t value = ~0U;
  for (size_t i = 0; i < size; ++i) {
    value ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320 : 0);
  }
  return ~value;
}
void put(uint8_t* out, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out[i] = uint8_t(value >> (8 * i));
}
uint32_t get(const uint8_t* in) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(in[i]) << (8 * i);
  return value;
}
}  // namespace
bool CredentialStore::open_() {
  if (fault_) return false;
  if (!opened_) opened_ = nvs_open("oa_network", NVS_READWRITE, &handle_) == ESP_OK;
  if (!opened_) fault_ = true;
  return opened_;
}
::openathan::LoadResult CredentialStore::read_(const char* key, uint8_t* bytes, size_t length) {
  using ::openathan::LoadResult;
  if (!open_()) return LoadResult::ERROR;
  size_t size = 0;
  auto result = nvs_get_blob(handle_, key, nullptr, &size);
  if (result == ESP_ERR_NVS_NOT_FOUND) return LoadResult::EMPTY;
  if (result != ESP_OK) {
    fault_ = true;
    return LoadResult::ERROR;
  }
  if (size != length) return LoadResult::ERROR;
  result = nvs_get_blob(handle_, key, bytes, &size);
  if (result != ESP_OK) {
    fault_ = true;
    return LoadResult::ERROR;
  }
  if (size != length || get(bytes + length - 4) != crc(bytes, length - 4)) return LoadResult::ERROR;
  return LoadResult::LOADED;
}
bool CredentialStore::write_(const char* key, uint8_t* bytes, size_t length) {
  if (!open_()) return false;
  put(bytes + length - 4, crc(bytes, length - 4));
  if (nvs_set_blob(handle_, key, bytes, length) != ESP_OK || nvs_commit(handle_) != ESP_OK) {
    fault_ = true;
    return false;
  }
  return true;
}
::openathan::LoadResult CredentialStore::load_wifi(WifiCredentials& value) {
  using ::openathan::LoadResult;
  std::array<uint8_t, 106> bytes{};
  const auto result = read_("wifi", bytes.data(), bytes.size());
  if (result != LoadResult::LOADED) return result;
  if (std::memcmp(bytes.data(), "OAW1", 4) || bytes[4] > 32 || bytes[5] > 64) return LoadResult::ERROR;
  WifiCredentials candidate{std::string(reinterpret_cast<char*>(bytes.data() + 6), bytes[4]),
                            std::string(reinterpret_cast<char*>(bytes.data() + 38), bytes[5])};
  if (!valid_wifi(candidate.ssid, candidate.password)) return LoadResult::ERROR;
  for (size_t i = 6 + bytes[4]; i < 38; ++i)
    if (bytes[i]) return LoadResult::ERROR;
  for (size_t i = 38 + bytes[5]; i < 102; ++i)
    if (bytes[i]) return LoadResult::ERROR;
  value = std::move(candidate);
  return LoadResult::LOADED;
}
bool CredentialStore::save_wifi(const WifiCredentials& value) {
  if (!valid_wifi(value.ssid, value.password)) return false;
  std::array<uint8_t, 106> bytes{'O', 'A', 'W', '1', uint8_t(value.ssid.size()), uint8_t(value.password.size())};
  std::copy(value.ssid.begin(), value.ssid.end(), bytes.begin() + 6);
  std::copy(value.password.begin(), value.password.end(), bytes.begin() + 38);
  return write_("wifi", bytes.data(), bytes.size());
}
::openathan::LoadResult CredentialStore::load_password(PasswordVerifier& value) {
  using ::openathan::LoadResult;
  std::array<uint8_t, 44> bytes{};
  const auto result = read_("password", bytes.data(), bytes.size());
  if (result != LoadResult::LOADED) return result;
  if (std::memcmp(bytes.data(), "OAP1", 4) || !get(bytes.data() + 4)) return LoadResult::ERROR;
  const std::string digest(reinterpret_cast<char*>(bytes.data() + 8), 32);
  if (!std::all_of(digest.begin(), digest.end(),
                   [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
    return LoadResult::ERROR;
  value = {get(bytes.data() + 4), digest};
  return LoadResult::LOADED;
}
bool CredentialStore::save_password(const PasswordVerifier& value) {
  if (!value.revision || value.ha1.size() != 32 || !std::all_of(value.ha1.begin(), value.ha1.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      }))
    return false;
  std::array<uint8_t, 44> bytes{'O', 'A', 'P', '1'};
  put(bytes.data() + 4, value.revision);
  std::copy(value.ha1.begin(), value.ha1.end(), bytes.begin() + 8);
  return write_("password", bytes.data(), bytes.size());
}
}  // namespace esphome::openathan_device
