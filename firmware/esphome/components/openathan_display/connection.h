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
  bool setup_active{}, setup_connecting{}, setup_error{};
  std::string setup_ssid, setup_ipv4;
};
struct ConnectionFrame {
  // ESPHome's 63-character device name plus .local and a terminator.
  std::array<char, 70> hostname{};
  std::array<char, 16> ipv4{};
  std::array<uint8_t, qrcodegen_BUFFER_LEN_FOR_VERSION(3)> qr{};
  bool available{}, connected{}, qr_valid{};
  std::array<char, 33> setup_ssid{};
  bool setup_active{}, setup_connecting{}, setup_error{};
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
inline bool encode_qr(ConnectionFrame &out, const char *payload) {
  std::array<uint8_t, qrcodegen_BUFFER_LEN_FOR_VERSION(3)> scratch{};
  const bool valid = qrcodegen_encodeText(payload, scratch.data(), out.qr.data(), qrcodegen_Ecc_MEDIUM,
                                        3, 3, qrcodegen_Mask_AUTO, false);
  if (!valid) out.qr = {};
  return valid;
}
inline ConnectionFrame connection_frame(const ConnectionInfo &info) {
  ConnectionFrame out;
  out.connected = info.connected;
  out.setup_active = info.setup_active || info.setup_connecting;
  out.setup_connecting = info.setup_connecting;
  out.setup_error = info.setup_error;
  if (out.setup_active) {
    // This separate display state represents the temporary open setup network,
    // not a working station connection or an existing owner's recovery mode.
    if (info.setup_ssid.empty() || info.setup_ssid.size() >= out.setup_ssid.size()) return out;
    for (const char ch : info.setup_ssid)
      if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-')) return out;
    std::copy(info.setup_ssid.begin(), info.setup_ssid.end(), out.setup_ssid.begin());
    out.available = true;
    if (info.setup_connecting) return out;  // The temporary AP is down during the join.
    if (info.setup_ipv4.size() >= out.ipv4.size() || !valid_ipv4(info.setup_ipv4)) return out;
    std::copy(info.setup_ipv4.begin(), info.setup_ipv4.end(), out.ipv4.begin());
    std::array<char, 51> payload{};
    const int length = std::snprintf(payload.data(), payload.size(), "WIFI:T:nopass;S:%s;;", out.setup_ssid.data());
    out.qr_valid = length > 0 && static_cast<size_t>(length) < payload.size() && encode_qr(out, payload.data());
    return out;
  }
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
  out.qr_valid = length > 0 && static_cast<size_t>(length) < url.size() && encode_qr(out, url.data());
  return out;
}
class ConnectionCache {
 public:
  const ConnectionFrame &read(const ConnectionInfo &info) {
    if (!valid_ || info.hostname != last_.hostname || info.ipv4 != last_.ipv4 || info.connected != last_.connected ||
        info.setup_active != last_.setup_active || info.setup_connecting != last_.setup_connecting ||
        info.setup_error != last_.setup_error || info.setup_ssid != last_.setup_ssid || info.setup_ipv4 != last_.setup_ipv4) {
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
