#pragma once
namespace esphome::openathan_storage {
#ifdef OPENATHAN_PROVISIONING_TEST_STORAGE
inline constexpr bool TEST_MODE=true;
#else
inline constexpr bool TEST_MODE=false;
#endif
}
