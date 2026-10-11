#pragma once
#include "esphome/components/json/json_util.h"
#include <string>
namespace esphome::openathan_device {
class QuranApi {
 public:
  virtual ~QuranApi() = default;
  virtual void snapshot(JsonObject root) = 0;
  virtual void catalog_snapshot(JsonObject root) = 0;
  virtual int action(const std::string &action, JsonObjectConst input, std::string &error) = 0;
};
}
