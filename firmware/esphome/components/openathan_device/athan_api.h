#pragma once
#include "esphome/components/json/json_util.h"
#include <string>

namespace esphome::openathan_device {
class AthanApi {
 public:
  virtual ~AthanApi() = default;
  virtual void snapshot(JsonObject root) = 0;
  virtual int action(const std::string &action, JsonObjectConst input, std::string &error) = 0;
};
}
