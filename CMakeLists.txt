cmake_minimum_required(VERSION 3.21)
project(openathan_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
file(GLOB ADHAN_SOURCES CONFIGURE_DEPENDS third_party/adhan-cpp/src/*.cpp)
add_library(openathan_core STATIC lib/openathan-core/src/prayer_calculator.cpp
  lib/openathan-core/src/scheduler.cpp lib/openathan-core/src/scheduler_state.cpp
  lib/openathan-core/src/settings.cpp ${ADHAN_SOURCES})
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
