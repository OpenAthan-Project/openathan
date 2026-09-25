#pragma once
#include "credential_store.h"
#include "protocol.h"

namespace esphome::openathan_device {
// The old connection (including the same SSID with an old password) must drop
// before a new join can authorize persisting the replacement credentials.
class WifiAttempt {
 public:
  enum class Result { WAIT, START_CONNECTION, CONNECTED, TIMED_OUT };
  bool begin(WifiCredentials candidate, uint64_t now) {
    if (active() || !valid_wifi(candidate.ssid, candidate.password)) return false;
    candidate_ = std::move(candidate);
    started_ = now;
    phase_ = 1;
    joined_ = false;
    return true;
  }
  bool active() const { return phase_ != 0; }
  const WifiCredentials& candidate() const { return candidate_; }
  void joined(const std::string& ssid) {
    if (phase_ == 2 && ssid == candidate_.ssid) joined_ = true;
  }
  Result step(uint64_t now, bool connected, const std::string& ssid) {
    if (!active()) return Result::WAIT;
    if (now < started_ || now - started_ >= 30000) return Result::TIMED_OUT;
    if (phase_ == 1 && !connected) {
      phase_ = 2;
      return Result::START_CONNECTION;
    }
    if (phase_ == 2 && joined_ && connected && ssid == candidate_.ssid) return Result::CONNECTED;
    return Result::WAIT;
  }
  void finish() {
    candidate_ = {};
    phase_ = 0;
    joined_ = false;
  }

 private:
  WifiCredentials candidate_;
  uint64_t started_{};
  unsigned phase_{};
  bool joined_{};
};
}  // namespace esphome::openathan_device
