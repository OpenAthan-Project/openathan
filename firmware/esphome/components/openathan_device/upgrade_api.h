#pragma once
#include "esphome/components/json/json_util.h"
#include <string>
namespace esphome::openathan_device {
class UpgradeApi {
 public:
  virtual ~UpgradeApi() = default;
  virtual void snapshot(JsonObject root) = 0;
  virtual int action(const std::string &action, JsonObjectConst input, std::string &error) = 0;
};
}  // namespace esphome::openathan_device
