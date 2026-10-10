#pragma once
namespace esphome {
namespace setup_priority { constexpr float AFTER_WIFI = 200; constexpr float LATE = -100; constexpr float HARDWARE = 800; constexpr float DATA = 600; constexpr float BEFORE_CONNECTION = 220; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void on_shutdown() {}
  virtual float get_setup_priority() const { return 0; }
  void mark_failed() { failed_ = true; }
  bool is_failed() const { return failed_; }
 private:
  bool failed_{false};
};
class PollingComponent : public Component {
 public:
  explicit PollingComponent(unsigned) {}
  virtual void update() {}
};
}
