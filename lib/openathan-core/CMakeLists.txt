# ESP-IDF component. The same sources are exercised by host tests at repo root.
file(GLOB ADHAN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../third_party/adhan-cpp/src/*.cpp")
idf_component_register(SRCS "src/prayer_calculator.cpp" ${ADHAN_SOURCES}
  INCLUDE_DIRS "include" "../../third_party/adhan-cpp/include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
