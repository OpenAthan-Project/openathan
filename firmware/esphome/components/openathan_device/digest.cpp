#include "digest.h"

#include <mbedtls/md5.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace esphome::openathan_device {
std::string md5_hex(const std::string& value) {
  uint8_t digest[16];
  if (mbedtls_md5(reinterpret_cast<const uint8_t*>(value.data()), value.size(), digest) != 0) return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(32);
  for (auto byte : digest) {
    out += hex[byte >> 4];
    out += hex[byte & 15];
  }
  return out;
}
namespace {
bool hex(const std::string& s, size_t size) {
  return s.size() == size &&
         std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool equal_secret(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned different = 0;
  for (size_t i = 0; i < a.size(); ++i) different |= uint8_t(a[i]) ^ uint8_t(b[i]);
  return different == 0;
}
bool parse(const std::string& header, std::map<std::string, std::string>& out) {
  if (header.size() > 2048 || header.compare(0, 7, "Digest ") != 0) return false;
  size_t at = 7;
  while (at < header.size()) {
    while (at < header.size() && header[at] == ' ') ++at;
    size_t start = at;
    while (at < header.size() && ((header[at] >= 'a' && header[at] <= 'z') || header[at] == '_')) ++at;
    const auto key = header.substr(start, at - start);
    if (key.empty() || at == header.size() || header[at++] != '=') return false;
    std::string value;
    if (at < header.size() && header[at] == '"') {
      ++at;
      bool closed = false;
      while (at < header.size()) {
        char c = header[at++];
        if (c == '"') {
          closed = true;
          break;
        }
        if (c == '\\') {
          if (at == header.size()) return false;
          c = header[at++];
        }
        if (uint8_t(c) < 32 || uint8_t(c) > 126) return false;
        value += c;
      }
      if (!closed) return false;
    } else {
      start = at;
      while (at < header.size() && header[at] != ',' && header[at] != ' ') ++at;
      value = header.substr(start, at - start);
    }
    if (!out.emplace(key, value).second) return false;
    while (at < header.size() && header[at] == ' ') ++at;
    if (at == header.size()) break;
    if (header[at++] != ',' || at == header.size()) return false;
  }
  return true;
}
}  // namespace
std::string DigestAuth::challenge(uint64_t now, const std::string& random_hex) {
  if (!available() || !hex(random_hex, 48)) return {};
  if (nonces_.size() >= 8) nonces_.pop_front();
  nonces_.push_back({random_hex, now, {}});
  return "Digest realm=\"" + realm_ + "\", nonce=\"" + random_hex + "\", algorithm=MD5, qop=\"auth\"";
}
bool DigestAuth::authorize(const std::string& header, const std::string& method, const std::string& uri, uint64_t now) {
  std::map<std::string, std::string> p;
  if (!available() || !parse(header, p) || p["username"] != "admin" || p["realm"] != realm_ || p["uri"] != uri ||
      p["qop"] != "auth" || (!p["algorithm"].empty() && p["algorithm"] != "MD5") || !hex(p["response"], 32) ||
      !hex(p["nc"], 8) || p["cnonce"].empty() || p["cnonce"].size() > 64)
    return false;
  auto found = std::find_if(nonces_.begin(), nonces_.end(), [&](const auto& n) { return n.value == p["nonce"]; });
  if (found == nonces_.end() || now < found->issued || now - found->issued >= 300000) return false;
  uint32_t count = 0;
  for (char c : p["nc"]) count = (count << 4) | uint32_t(c <= '9' ? c - '0' : c - 'a' + 10);
  if (!count) return false;
  auto client = found->clients.find(p["cnonce"]);
  if (client == found->clients.end() && found->clients.size() >= 8) return false;
  const Replay previous = client == found->clients.end() ? Replay{} : client->second;
  if (count <= previous.highest &&
      (previous.highest - count >= 32 || (previous.seen & (1U << (previous.highest - count)))))
    return false;
  const auto expected = md5_hex(verifier_ + ":" + p["nonce"] + ":" + p["nc"] + ":" + p["cnonce"] +
                                ":auth:" + md5_hex(method + ":" + uri));
  if (!equal_secret(expected, p["response"])) return false;
  auto next = previous;
  if (count > next.highest) {
    const auto distance = count - next.highest;
    next.seen = distance >= 32 ? 1 : (next.seen << distance) | 1;
    next.highest = count;
  } else
    next.seen |= 1U << (next.highest - count);
  found->clients[p["cnonce"]] = next;
  return true;
}
bool same_origin(const std::string& origin, const std::string& host, const std::string& site) {
  return !origin.empty() && origin == "http://" + host && (site.empty() || site == "same-origin" || site == "none");
}
}  // namespace esphome::openathan_device
