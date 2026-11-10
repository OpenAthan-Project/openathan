#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace esphome::openathan_device::speaker_finder {
inline constexpr char alias[] = "openathan";
inline constexpr char host[] = "openathan.local";
inline constexpr size_t max_speakers = 16;

struct Speaker {
  std::string hostname;
  std::string name;
  bool candidate{};
  bool owner{};
};

// Hostnames here are DNS-SD target labels without the .local suffix.
bool valid_hostname(std::string_view hostname);
void record(std::vector<Speaker> &speakers, Speaker speaker);

enum class Action { WAIT, CHECK_ALIAS, YIELD };
class Election {
 public:
  Action observe(std::string_view self, bool owner, const std::vector<Speaker> &peers);

 private:
  unsigned quiet_rounds_{};
};

std::string page(const std::vector<Speaker> &speakers);
}  // namespace esphome::openathan_device::speaker_finder
