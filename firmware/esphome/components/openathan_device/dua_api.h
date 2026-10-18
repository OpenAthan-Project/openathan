#pragma once
#include "esphome/components/json/json_util.h"
#include <string>
namespace esphome::openathan_device {
class DuaApi {
 public:
  virtual ~DuaApi() = default;
  virtual void preference_changed() {}
  virtual void snapshot(JsonObject root) = 0;
  virtual int retry(std::string &error) = 0;
};
}
