#pragma once
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/es8311_playback/audio.h"
#include <array>
#include <functional>

namespace esphome::waveshare_audio_board {
// Sole owner of the TCA9555. V1.1: P10=PA, P11/P12/P13=KEY1/2/3.
// Only bank 1 is touched; preserve every unrelated output, direction and polarity.
class Board : public Component, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::IO; }
  void set_key_handler(std::function<void(unsigned)> handler) { handler_ = std::move(handler); }
  void setup() override {
    uint8_t modes{}, polarity{};
    if (!read_byte(3, &outputs_) || !read_byte(7, &modes) || !read_byte(5, &polarity)) {
      mark_failed(); return;
    }
    // Load the low output latch BEFORE enabling the amplifier output direction.
    outputs_ &= ~1U;
    if (!checked_write_(3, outputs_) || !checked_write_(5, polarity & ~0x0EU) ||
        !checked_write_(7, (modes | 0x0EU) & ~1U)) { fail_(); return; }
    initialized_ = true;
  }
  bool ready() const { return initialized_ && !is_failed(); }
  bool amplifier(bool enabled) {
    if (!ready()) return false;
    const uint8_t next = enabled ? outputs_ | 1U : outputs_ & ~1U;
    if (!checked_write_(3, next)) { fail_(); return false; }
    outputs_ = next;
    return true;
  }
  void loop() override { poll(millis()); }
  void poll(uint32_t now) {
    if (!ready() || static_cast<uint32_t>(now - last_poll_) < 20) return;
    last_poll_ = now;
    uint8_t inputs{};
    if (!read_byte(1, &inputs)) {
      // Never turn a transport failure into an active-low key press. Require a
      // stable release again after recovery, including keys held through failure.
      keys_ = {}; return;
    }
    for (unsigned index = 0; index < keys_.size(); ++index) {
      auto &key = keys_[index];
      const bool pressed = !(inputs & (1U << (index + 1)));
      if (!key.sampled || pressed != key.raw) {
        key.sampled = true; key.raw = pressed; key.since = now;
      }
      if (static_cast<uint32_t>(now - key.since) < 50) continue;
      if (!pressed) key.armed = true;
      else if (key.armed) {
        key.armed = false;
        if (handler_) handler_(index);
      }
    }
  }
  void on_shutdown() override { if (ready()) amplifier(false); }
 private:
  bool checked_write_(uint8_t reg, uint8_t value) {
    uint8_t actual{};
    return write_byte(reg, value) && read_byte(reg, &actual) && actual == value;
  }
  void fail_() {
    // Best effort only: a failed bus cannot guarantee physical amplifier state.
    outputs_ &= ~1U;
    write_byte(3, outputs_);
    mark_failed();
  }
  struct Key { bool sampled{}, raw{}, armed{}; uint32_t since{}; };
  std::array<Key, 3> keys_{};
  std::function<void(unsigned)> handler_;
  uint32_t last_poll_{};
  uint8_t outputs_{};
  bool initialized_{};
};

class Audio : public es8311_playback::Audio {
 public:
  void set_board(Board *board) { board_ = board; }
  bool ready() const override { return board_ && board_->ready() && es8311_playback::Audio::ready(); }
  void loop() override {
    if (!board_ || !board_->ready()) mark_failed();
    es8311_playback::Audio::loop();
  }
 protected:
  bool set_amplifier_(bool value) override { return board_ && board_->amplifier(value); }
 private:
  Board *board_{};
};
}
