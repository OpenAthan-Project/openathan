#pragma once
namespace esphome {
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  void mark_failed() { failed_ = true; }
  bool is_failed() const { return failed_; }
 private:
  bool failed_{false};
};
}
