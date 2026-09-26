#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

namespace esphome::openathan_device {
std::string md5_hex(const std::string& value);
class DigestAuth {
 public:
  enum class Result { ACCEPTED, REJECTED, STALE };
  void configure(std::string realm, std::string verifier) {
    realm_ = std::move(realm);
    verifier_ = std::move(verifier);
    nonces_.clear();
  }
  bool available() const { return !verifier_.empty(); }
  std::string challenge(uint64_t now, const std::string& random_hex, bool stale = false);
  Result authorize(const std::string& header, const std::string& method, const std::string& uri, uint64_t now);

 private:
  struct Replay {
    uint32_t highest{}, seen{};
  };
  struct Nonce {
    std::string value;
    uint64_t issued;
    // nc counts uses of this server nonce, even when the client rotates cnonce.
    Replay replay;
  };
  std::string realm_, verifier_;
  std::deque<Nonce> nonces_;
};
bool same_origin(const std::string& origin, const std::string& host, const std::string& site);
}  // namespace esphome::openathan_device
