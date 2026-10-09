#pragma once
#include "openathan/scheduler.h"
#include "openathan/time_format.h"
#include <array>
#include <cstdio>
#include <string_view>

namespace openathan::screen {
struct Inputs {
  SchedulerStatus status;
  bool setup_complete{}, storage_fault{}, clock_valid{}, wifi_connected{}, prayers_enabled{};
  std::string_view local_time, next_time;
  uint8_t hours{24};
  bool stop_button{true};
};
struct Frame {
  std::array<char, 9> clock{};
  std::array<char, 3> meridiem{};
  std::array<char, 9> heading{}, main{};
  std::array<char, 16> detail{}, footer{};
  unsigned main_scale{2};
  bool error{}, upcoming{};
  bool operator==(const Frame &) const = default;
};
template<size_t N> inline void text(std::array<char, N> &out, std::string_view value) {
  const auto count = value.size() < N - 1 ? value.size() : N - 1;
  for (size_t i = 0; i < count; ++i) out[i] = value[i];
  out[count] = '\0';
}
inline bool valid_time(std::string_view value) {
  return value.size() == 5 && value[2] == ':' && value[0] >= '0' && value[0] <= '2' &&
      value[1] >= '0' && value[1] <= '9' && (value[0] != '2' || value[1] <= '3') &&
      value[3] >= '0' && value[3] <= '5' && value[4] >= '0' && value[4] <= '9';
}
inline Frame present(const Inputs &in) {
  Frame f;
  f.clock = format_clock(in.clock_valid ? in.local_time : std::string_view{}, in.hours);
  if (!f.clock[0]) text(f.clock, "--:--");
  if (!in.wifi_connected) text(f.footer, "Offline");
  if (in.storage_fault || in.status.fault != Fault::NONE) {
    f.error = true;
    text(f.heading, "Error");
    text(f.main, in.storage_fault || in.status.fault == Fault::STORAGE ? "Storage" :
        in.status.fault == Fault::AUDIO_UNAVAILABLE || in.status.fault == Fault::PLAYBACK_REJECTED ? "Audio" : "Schedule");
    text(f.detail, "Use device page");
  } else if (!in.setup_complete) {
    text(f.heading, "Setup"); text(f.main, "Needed"); text(f.detail, "Use phone setup");
  } else if (in.status.playing) {
    text(f.heading, "Athan"); text(f.main, "Playing"); text(f.detail, in.stop_button ? "Button to stop" : "Use phone");
  } else if (!in.clock_valid) {
    text(f.heading, "Time"); text(f.main, "Waiting"); text(f.detail, in.wifi_connected ? "Syncing clock" : "Connect Wi-Fi");
  } else if (!in.prayers_enabled) {
    text(f.heading, "Athan"); text(f.main, "Off"); text(f.detail, "All prayers off");
  } else if (in.status.next && valid_time(in.next_time)) {
    f.upcoming = true;
    text(f.heading, prayer_name(in.status.next->key.prayer)); const auto formatted = format_clock(in.next_time, in.hours);
    const std::string_view time(formatted.data());
    text(f.main, time.substr(0, time.find(' '))); f.main_scale = 3;
    if (in.hours == 12) text(f.meridiem, time.substr(time.size() - 2));
    const bool skipped = in.status.skip && (*in.status.skip == in.status.next->key ||
        (in.status.next->shared_with && *in.status.skip == *in.status.next->shared_with));
    text(f.detail, skipped ? "Will be skipped" : in.status.automatic_ready ? "Next Athan" : "Not ready yet");
  } else {
    text(f.heading, "Athan"); text(f.main, "Waiting"); text(f.detail, "No time set");
  }
  return f;
}
// Do not send unchanged frames over SPI, including one-second polls within a minute.
class FrameCache {
 public:
  bool accept(const Frame &frame) {
    if (valid_ && frame == frame_) return false;
    frame_ = frame; valid_ = true; return true;
  }
  const Frame &frame() const { return frame_; }
 private:
  Frame frame_{};
  bool valid_{};
};
}  // namespace openathan::screen
