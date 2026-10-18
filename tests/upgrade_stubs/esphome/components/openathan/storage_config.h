#pragma once
namespace esphome::openathan_storage {
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
inline constexpr const char *PRAYER="oa_validation";
inline constexpr bool TEST_MODE=true;
#else
inline constexpr const char *PRAYER="openathan";
inline constexpr bool TEST_MODE=false;
#endif
}
