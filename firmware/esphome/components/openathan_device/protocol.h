#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace esphome::openathan_device {
struct Frame {
  bool extension{};
  uint8_t type{};
  std::vector<uint8_t> data;
};
class SerialParser {
 public:
  std::optional<Frame> feed(uint8_t byte, uint64_t now);

 private:
  std::vector<uint8_t> bytes_;
  uint64_t last_{};
};
std::vector<uint8_t> frame(bool extension, uint8_t type, const std::vector<uint8_t>& data);
std::vector<uint8_t> rpc(uint8_t command, const std::vector<std::string>& fields);
bool parse_rpc(const std::vector<uint8_t>& data, uint8_t& command, std::vector<std::string>& fields);
bool valid_wifi(const std::string& ssid, const std::string& password);
bool valid_password(const std::string& password);
}  // namespace esphome::openathan_device
