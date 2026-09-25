#pragma once
#include <deque>
#include <functional>
#include <map>
#include <string>

namespace esphome::openathan_device {
std::string md5_hex(const std::string& value);
class DigestAuth {
 public:
  void configure(std::string realm, std::string verifier) {
    realm_ = std::move(realm);
    verifier_ = std::move(verifier);
    nonces_.clear();
  }
  bool available() const { return !verifier_.empty(); }
  std::string challenge(uint64_t now, const std::string& random_hex);
  bool authorize(const std::string& header, const std::string& method, const std::string& uri, uint64_t now);

 private:
  struct Replay {
    uint32_t highest{}, seen{};
  };
  struct Nonce {
    std::string value;
    uint64_t issued;
    std::map<std::string, Replay> clients;
  };
  std::string realm_, verifier_;
  std::deque<Nonce> nonces_;
};
bool same_origin(const std::string& origin, const std::string& host, const std::string& site);
}  // namespace esphome::openathan_device
