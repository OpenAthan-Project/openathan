#pragma once
#include <vector>
namespace esphome {
class GPIOPin {
 public:
  bool initialized{};
  std::vector<bool> levels;
  void setup() { initialized = true; }
  void digital_write(bool value) { levels.push_back(value); }
};
}
