#pragma once
#include "esphome/core/defines.h"

namespace esphome::openathan_storage {
// Selected for the entire application at build time, never through a request.
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
inline constexpr bool TEST_MODE = true;
inline constexpr const char* PRAYER = "oa_test";
inline constexpr const char* SETUP = "oa_setup_test";
inline constexpr const char* NETWORK = "oa_network_test";
#else
inline constexpr bool TEST_MODE = false;
inline constexpr const char* PRAYER = "openathan";
inline constexpr const char* SETUP = "oa_setup";
inline constexpr const char* NETWORK = "oa_network";
#endif
}  // namespace esphome::openathan_storage
