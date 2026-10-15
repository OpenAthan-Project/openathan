#pragma once
#include "qrcodegen.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace openathan::screen {
enum class Page { STATUS, QR, ADDRESS };
// Product networking supplies this read-only snapshot; neither the renderer nor
// the reusable prayer engine owns credentials or provisioning.
struct ConnectionInfo {
  std::string hostname, ipv4;
  bool connected{}, playback_busy{};
};
struct ConnectionFrame {
  // ESPHome's 63-character device name plus .local and a terminator.
  std::array<char, 70> hostname{};
  std::array<char, 16> ipv4{};
  std::array<uint8_t, qrcodegen_BUFFER_LEN_FOR_VERSION(3)> qr{};
  bool available{}, connected{}, qr_valid{};
  bool operator==(const ConnectionFrame &) const = default;
};
inline bool valid_ipv4(std::string_view value) {
  unsigned groups = 0, octet = 0, digits = 0;
  for (const char ch : value) {
    if (ch >= '0' && ch <= '9') {
      octet = octet*10 + static_cast<unsigned>(ch-'0');
      if (++digits > 3 || octet > 255) return false;
    } else if (ch == '.' && digits && groups < 3) {
      ++groups; octet = digits = 0;
    } else return false;
  }
  return groups == 3 && digits && value != "0.0.0.0";
}
inline ConnectionFrame connection_frame(const ConnectionInfo &info) {
  ConnectionFrame out;
  out.connected = info.connected;
  // Reject rather than truncate an address. A malformed/unsupported identity
  // must never produce a QR pointing at a different host.
  if (info.hostname.empty() || info.hostname.size() >= out.hostname.size()) return out;
  for (const char ch : info.hostname)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.')) return out;
  out.available = true;
  std::copy(info.hostname.begin(), info.hostname.end(), out.hostname.begin());
  if (!info.connected) return out;
  if (info.ipv4.size() < out.ipv4.size() && valid_ipv4(info.ipv4))
    std::copy(info.ipv4.begin(), info.ipv4.end(), out.ipv4.begin());
  if (!out.ipv4[0]) return out;
  std::array<char, 78> url{};
  const int length = std::snprintf(url.data(), url.size(), "http://%s/", out.hostname.data());
  std::array<uint8_t, qrcodegen_BUFFER_LEN_FOR_VERSION(3)> scratch{};
  out.qr_valid = length > 0 && static_cast<size_t>(length) < url.size() &&
      qrcodegen_encodeText(url.data(), scratch.data(), out.qr.data(), qrcodegen_Ecc_MEDIUM,
                          3, 3, qrcodegen_Mask_AUTO, false);
  if (!out.qr_valid) out.qr = {};
  return out;
}
class ConnectionCache {
 public:
  const ConnectionFrame &read(const ConnectionInfo &info) {
    if (!valid_ || info.hostname != last_.hostname || info.ipv4 != last_.ipv4 || info.connected != last_.connected) {
      frame_ = connection_frame(info);
      last_ = info;
      valid_ = true;
    }
    return frame_;
  }
 private:
  ConnectionInfo last_;
  ConnectionFrame frame_;
  bool valid_{};
};
}  // namespace openathan::screen
