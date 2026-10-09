cmake_minimum_required(VERSION 3.21)
project(openathan_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(upgrade_policy_tests tests/upgrade_policy_tests.cpp)
target_include_directories(upgrade_policy_tests PRIVATE firmware/esphome/components/openathan_device lib/openathan-core/include)
add_test(NAME upgrade_safe_window COMMAND upgrade_policy_tests)
file(GLOB ADHAN_SOURCES CONFIGURE_DEPENDS third_party/adhan-cpp/src/*.cpp)
add_library(openathan_core STATIC lib/openathan-core/src/prayer_calculator.cpp
  lib/openathan-core/src/scheduler.cpp lib/openathan-core/src/scheduler_state.cpp
  lib/openathan-core/src/settings.cpp lib/openathan-core/src/lights.cpp lib/openathan-core/src/time_format.cpp lib/openathan-core/src/display.cpp ${ADHAN_SOURCES})
target_include_directories(openathan_core PUBLIC lib/openathan-core/include third_party/adhan-cpp/include)
add_executable(prayer_tests tests/prayer_tests.cpp)
target_link_libraries(prayer_tests PRIVATE openathan_core)
add_test(NAME prayer_reference COMMAND prayer_tests)
add_executable(scheduler_tests tests/scheduler_tests.cpp)
target_link_libraries(scheduler_tests PRIVATE openathan_core)
add_test(NAME scheduler_scenarios COMMAND scheduler_tests)
add_executable(nvs_state_tests tests/nvs_state_tests.cpp
  firmware/esphome/components/openathan/nvs_state_store.cpp)
target_include_directories(nvs_state_tests PRIVATE tests/stubs firmware/esphome/components/openathan)
target_link_libraries(nvs_state_tests PRIVATE openathan_core)
add_test(NAME nvs_durability COMMAND nvs_state_tests)
add_executable(settings_tests tests/settings_tests.cpp
  firmware/esphome/components/openathan/nvs_settings_store.cpp
  firmware/esphome/components/openathan/nvs_state_store.cpp)
target_include_directories(settings_tests PRIVATE tests/stubs firmware/esphome/components/openathan)
target_link_libraries(settings_tests PRIVATE openathan_core)
add_test(NAME settings_durability COMMAND settings_tests)
add_executable(audio_validator tests/audio_validator.cpp)
target_include_directories(audio_validator PRIVATE lib/openathan-core/include)
add_executable(audio_adapter_tests tests/audio_adapter_tests.cpp
  firmware/esphome/components/openathan_audio/partition_audio.cpp)
target_include_directories(audio_adapter_tests PRIVATE tests/stubs lib/openathan-core/include
  firmware/esphome/components/openathan_audio)
if(NOT APPLE)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(audio_validator PRIVATE OpenSSL::Crypto)
  target_link_libraries(audio_adapter_tests PRIVATE OpenSSL::Crypto)
endif()

add_executable(provisioning_tests tests/provisioning_tests.cpp
  firmware/esphome/components/openathan/setup_store.cpp
  firmware/esphome/components/openathan_device/protocol.cpp
  firmware/esphome/components/openathan_device/digest.cpp
  firmware/esphome/components/openathan_device/credential_store.cpp)
target_include_directories(provisioning_tests PRIVATE tests/stubs
  firmware/esphome/components/openathan firmware/esphome/components/openathan_device)
target_link_libraries(provisioning_tests PRIVATE openathan_core)
if(NOT APPLE)
  target_link_libraries(provisioning_tests PRIVATE OpenSSL::Crypto)
endif()
add_test(NAME provisioning_safety COMMAND provisioning_tests)

add_executable(digest_test_bridge tests/digest_test_bridge.cpp
  firmware/esphome/components/openathan_device/digest.cpp)
target_include_directories(digest_test_bridge PRIVATE tests/stubs firmware/esphome/components/openathan_device)
if(NOT APPLE)
  target_link_libraries(digest_test_bridge PRIVATE OpenSSL::Crypto)
endif()

foreach(mode IN ITEMS production isolated)
  add_executable(storage_${mode}_tests tests/storage_isolation_tests.cpp
    firmware/esphome/components/openathan/nvs_settings_store.cpp
    firmware/esphome/components/openathan/nvs_state_store.cpp
    firmware/esphome/components/openathan/setup_store.cpp
    firmware/esphome/components/openathan_device/credential_store.cpp
    firmware/esphome/components/openathan_device/protocol.cpp
    firmware/esphome/components/openathan_device/test_maintenance.cpp)
  target_include_directories(storage_${mode}_tests PRIVATE tests/stubs
    firmware/esphome/components/openathan firmware/esphome/components/openathan_device)
  target_link_libraries(storage_${mode}_tests PRIVATE openathan_core)
  if(mode STREQUAL "isolated")
    target_compile_definitions(storage_${mode}_tests PRIVATE OPENATHAN_PROVISIONING_TEST_STORAGE)
  endif()
  add_test(NAME storage_${mode} COMMAND storage_${mode}_tests)
endforeach()

foreach(mode IN ITEMS production isolated)
  add_executable(lights_${mode}_tests tests/light_tests.cpp
    firmware/esphome/components/openathan/nvs_settings_store.cpp)
  target_include_directories(lights_${mode}_tests PRIVATE tests/stubs firmware/esphome/components/openathan)
  target_link_libraries(lights_${mode}_tests PRIVATE openathan_core)
  if(mode STREQUAL "isolated")
    target_compile_definitions(lights_${mode}_tests PRIVATE OPENATHAN_PROVISIONING_TEST_STORAGE)
  endif()
  add_test(NAME lights_${mode} COMMAND lights_${mode}_tests)
endforeach()

add_executable(light_adapter_tests tests/light_adapter_tests.cpp
  firmware/esphome/components/voice_pyramid/lights.cpp)
target_include_directories(light_adapter_tests PRIVATE tests/stubs lib/openathan-core/include
  firmware/esphome/components/voice_pyramid)
target_compile_definitions(light_adapter_tests PRIVATE USE_OPENATHAN_LIGHTS)
add_test(NAME light_register_safety COMMAND light_adapter_tests)

add_executable(display_tests tests/display_tests.cpp
  firmware/esphome/components/atom_s3r_display/backlight.cpp)
target_include_directories(display_tests PRIVATE tests/stubs lib/openathan-core/include
  firmware/esphome/components/openathan_display firmware/esphome/components/atom_s3r_display)
target_link_libraries(display_tests PRIVATE openathan_core)
add_test(NAME display_status_and_backlight COMMAND display_tests)

add_executable(waveshare_adapter_tests tests/waveshare_adapter_tests.cpp)
target_include_directories(waveshare_adapter_tests PRIVATE tests/waveshare_stubs tests/stubs
  firmware lib/openathan-core/include)
add_test(NAME waveshare_power_and_backlight COMMAND waveshare_adapter_tests)

foreach(mode IN ITEMS production isolated)
  add_executable(display_preferences_${mode}_tests tests/display_preferences_tests.cpp
    firmware/esphome/components/openathan/nvs_settings_store.cpp)
  target_include_directories(display_preferences_${mode}_tests PRIVATE tests/stubs firmware/esphome/components/openathan)
  target_link_libraries(display_preferences_${mode}_tests PRIVATE openathan_core)
  if(mode STREQUAL "isolated")
    target_compile_definitions(display_preferences_${mode}_tests PRIVATE OPENATHAN_PROVISIONING_TEST_STORAGE)
  endif()
  add_test(NAME display_preferences_${mode} COMMAND display_preferences_${mode}_tests)
endforeach()

foreach(mode IN ITEMS production isolated)
  add_executable(time_format_${mode}_tests tests/time_format_tests.cpp
    firmware/esphome/components/openathan/nvs_settings_store.cpp)
  target_include_directories(time_format_${mode}_tests PRIVATE tests/stubs firmware/esphome/components/openathan)
  target_link_libraries(time_format_${mode}_tests PRIVATE openathan_core)
  if(mode STREQUAL "isolated")
    target_compile_definitions(time_format_${mode}_tests PRIVATE OPENATHAN_PROVISIONING_TEST_STORAGE)
  endif()
  add_test(NAME time_format_${mode} COMMAND time_format_${mode}_tests)
endforeach()
