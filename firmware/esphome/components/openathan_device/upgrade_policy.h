#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace esphome::openathan_device {
inline std::optional<std::array<uint32_t, 3>> release_version(std::string_view value) {
  if (value.empty() || value.front() != 'v') return {};
  value.remove_prefix(1);
  std::array<uint32_t, 3> parts{};
  for (unsigned i = 0; i < 3; ++i) {
    auto end = i == 2 ? value.size() : value.find('.');
    if (end == std::string_view::npos || end == 0 || (end > 1 && value[0] == '0')) return {};
    auto [ptr, error] = std::from_chars(value.data(), value.data() + end, parts[i]);
    if (error != std::errc{} || ptr != value.data() + end) return {};
    value.remove_prefix(end);
    if (i != 2) value.remove_prefix(1);
  }
  return parts;
}
inline bool newer_release(std::string_view candidate, std::string_view current) {
  const auto a = release_version(candidate), b = release_version(current);
  return a && b && *a > *b;
}
// The main loop publishes this decision to the I/O task; no scheduler objects
// are read on another task. Protect both staging and the eventual restart.
inline bool upgrade_window(bool playing, bool clock, bool activated, bool ready,
                           int64_t now, std::optional<int64_t> next) {
  if (playing || !clock) return false;
  if (!activated) return true;
  return ready && (!next || (*next > now && *next - now >= 900));
}
}  // namespace esphome::openathan_device
