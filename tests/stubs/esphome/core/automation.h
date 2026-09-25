#pragma once
namespace esphome {
template<typename... Ts> class Action {
 public:
  virtual ~Action() = default;
  virtual void play(const Ts &...args) = 0;
};
}
