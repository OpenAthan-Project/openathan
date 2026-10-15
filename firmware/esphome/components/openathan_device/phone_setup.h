#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace esphome::openathan_device::phone_setup {
inline constexpr std::string_view IP = "192.168.4.1";
inline constexpr std::string_view URL = "http://192.168.4.1/setup/";
enum class State { CLOSED, READY, CONNECTING, FAILED, STORAGE_FAULT };
inline const char *state_name(State state) {
  switch (state) {
    case State::CONNECTING: return "connecting";
    case State::FAILED: return "failed";
    case State::STORAGE_FAULT: return "storage_fault";
    default: return "ready";
  }
}
inline bool eligible(bool incomplete, bool healthy, bool wifi_empty, bool password_fault) {
  return incomplete && healthy && wifi_empty && !password_fault;
}
inline bool ap_socket(bool active, std::string_view local_ip, std::string_view station_ip) {
  return active && local_ip == IP && station_ip.empty();
}
inline bool canonical_host(std::string_view host) { return host == IP || host == "192.168.4.1:80"; }
inline bool setup_path(std::string_view path) { return path == "/setup" || path.starts_with("/setup/"); }
inline bool equal_verifier(std::string_view actual, std::string_view expected) {
  if (actual.size() != 32 || expected.size() != 32) return false;
  unsigned difference = 0;
  for (size_t i = 0; i < 32; ++i) difference |= static_cast<unsigned char>(actual[i]) ^ static_cast<unsigned char>(expected[i]);
  return difference == 0;
}
// One ordinary, uncompressed IN question. No reflection of arbitrary payloads,
// compression-pointer traversal, multiple-question amplification or EDNS data.
size_t dns_response(std::span<const uint8_t> query, std::span<uint8_t> output);
}  // namespace esphome::openathan_device::phone_setup
