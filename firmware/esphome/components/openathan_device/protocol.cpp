#include "protocol.h"

#include <algorithm>

namespace esphome::openathan_device {
std::optional<Frame> SerialParser::feed(uint8_t byte, uint64_t now) {
  if (now < last_ || now - last_ > 1000) bytes_.clear();
  last_ = now;
  bytes_.push_back(byte);
  constexpr const char* headers[] = {"IMPROV", "OATHAN"};
  if (bytes_.size() <= 6) {
    while (!bytes_.empty() && !std::equal(bytes_.begin(), bytes_.end(), headers[0]) &&
           !std::equal(bytes_.begin(), bytes_.end(), headers[1]))
      bytes_.erase(bytes_.begin());
    return {};
  }
  if (bytes_[6] != 1) {
    bytes_.clear();
    return {};
  }
  if (bytes_.size() < 9 || bytes_.size() < size_t(bytes_[8]) + 10) return {};
  uint8_t sum = 0;
  for (size_t i = 0; i + 1 < bytes_.size(); ++i) sum += bytes_[i];
  std::optional<Frame> result;
  if (sum == bytes_.back()) result = Frame{bytes_[0] == 'O', bytes_[7], {bytes_.begin() + 9, bytes_.end() - 1}};
  std::fill(bytes_.begin(), bytes_.end(), 0);
  bytes_.clear();
  return result;
}
std::vector<uint8_t> frame(bool extension, uint8_t type, const std::vector<uint8_t>& data) {
  if (data.size() > 255) return {};
  const std::string header = extension ? "OATHAN" : "IMPROV";
  std::vector<uint8_t> out(header.begin(), header.end());
  out.insert(out.end(), {1, type, uint8_t(data.size())});
  out.insert(out.end(), data.begin(), data.end());
  uint8_t sum = 0;
  for (auto byte : out) sum += byte;
  out.push_back(sum);
  out.push_back('\n');
  return out;
}
std::vector<uint8_t> rpc(uint8_t command, const std::vector<std::string>& fields) {
  std::vector<uint8_t> out{command, 0};
  for (const auto& field : fields) {
    if (field.size() > 251 || out.size() + 1 + field.size() > 255) return {};
    out.push_back(uint8_t(field.size()));
    out.insert(out.end(), field.begin(), field.end());
  }
  out[1] = uint8_t(out.size() - 2);
  return out;
}
bool parse_rpc(const std::vector<uint8_t>& data, uint8_t& command, std::vector<std::string>& fields) {
  fields.clear();
  if (data.size() < 2 || size_t(data[1]) + 2 != data.size()) return false;
  command = data[0];
  for (size_t at = 2; at < data.size();) {
    const size_t size = data[at++];
    if (at + size > data.size()) return false;
    fields.emplace_back(data.begin() + at, data.begin() + at + size);
    if (fields.back().find('\0') != std::string::npos) return false;
    at += size;
  }
  return true;
}
bool valid_wifi(const std::string& ssid, const std::string& password) {
  if (ssid.empty() || ssid.size() > 32 || ssid.find('\0') != std::string::npos ||
      password.find('\0') != std::string::npos)
    return false;
  if (password.empty()) return false;
  if (password.size() == 64)
    return std::all_of(password.begin(), password.end(), [](unsigned char c) {
      return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
  return password.size() >= 8 && password.size() <= 63;
}
bool valid_password(const std::string& password) {
  return password.size() >= 12 && password.size() <= 128 &&
         std::all_of(password.begin(), password.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
}
}  // namespace esphome::openathan_device
