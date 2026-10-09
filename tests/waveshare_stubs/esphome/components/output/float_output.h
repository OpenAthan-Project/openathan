#pragma once
#include <vector>
namespace esphome::output {
class FloatOutput {
 public:
  std::vector<float> levels;
  void set_level(float value) { levels.push_back(value); }
};
}
