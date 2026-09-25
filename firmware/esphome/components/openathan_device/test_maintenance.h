#pragma once
#include <functional>
#include <string>
#include <vector>

namespace esphome::openathan_device {
enum class ClearResult { INVALID, CLEARED, STORAGE };
// USB-only test operation. Production builds always return INVALID.
ClearResult clear_test_storage(const std::vector<std::string>& fields, const std::string& hostname,
                               const std::function<void()>& quiesce);
}  // namespace esphome::openathan_device
