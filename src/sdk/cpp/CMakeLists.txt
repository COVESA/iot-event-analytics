cmake_minimum_required(VERSION 3.11)

project(iotea-sdk-cpp)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "PIC")

set(IOTEA_SDK_STATIC_PAHO TRUE CACHE BOOL "Statically link Paho libraries")

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.10.0
)

FetchContent_Declare(
  paho_mqtt_c
  GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c
  GIT_TAG        v1.3.8
)

FetchContent_Declare(
  paho_mqtt_cpp
  GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp
  GIT_TAG        076fc9977d07c25fa9fef1e19026d8be069c8e94
)

FetchContent_Declare(
  nlohmann_json
  # https://github.com/nlohmann/json.git is huge (254 MB).
  # Use mirror intended for purpose instead
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
  GIT_TAG        v3.9.1
)

# Paho MQTT C
FetchContent_Populate(paho_mqtt_c)
set(PAHO_WITH_SSL       FALSE CACHE STRING "With SSL")
set(PAHO_ENABLE_TESTING OFF   CACHE STRING "Enable Paho testing")
if(IOTEA_SDK_STATIC_PAHO)
  set(PAHO_BUILD_SHARED FALSE CACHE STRING "Paho shared library")
  set(PAHO_BUILD_STATIC TRUE  CACHE STRING "Paho static library")
else()
  set(PAHO_BUILD_SHARED TRUE  CACHE STRING "Paho shared library")
  set(PAHO_BUILD_STATIC FALSE CACHE STRING "Paho static library")
endif()
add_subdirectory(${paho_mqtt_c_SOURCE_DIR} ${paho_mqtt_c_BINARY_DIR})

# Paho MQTT C++
FetchContent_Populate(paho_mqtt_cpp)
add_subdirectory(${paho_mqtt_cpp_SOURCE_DIR} ${paho_mqtt_cpp_BINARY_DIR})

# nlohmann json
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Populate(nlohmann_json)
add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)

# Google Test
FetchContent_Populate(googletest)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

## enable testing
include(CTest)

## adapters
add_subdirectory(adapters)

## examples
add_subdirectory(examples)

## lib
add_subdirectory(lib)

