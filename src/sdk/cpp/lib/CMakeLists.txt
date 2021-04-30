## cmake flags
cmake_minimum_required(VERSION 3.10)

## project name
project("iotea-sdk-cpp-lib"
    VERSION "0.0.1"
    LANGUAGES CXX
)

## Generate compile_commands.json (for IDE code navigation etc)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## C++ flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate position-independent code (-fPIC on UNIX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# System Libraries
include(GNUInstallDirs)

# Code coverage
include(code-coverage)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(LIBS_SYSTEM ws2_32)
elseif(UNIX)
    set(LIBS_SYSTEM c stdc++)
endif()

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(iotea_sdk_cpp
  SHARED
    src/util.cpp
    src/logging.cpp
    src/iotea.cpp
    src/schema.cpp
    src/talent_test.cpp
    src/mqtt_client.cpp
    src/jsonquery.cpp
)
target_code_coverage(iotea_sdk_cpp)

## link binaries

target_include_directories(iotea_sdk_cpp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(IOTEA_SDK_STATIC_PAHO)
  target_link_libraries(iotea_sdk_cpp
    PUBLIC
      paho-mqttpp3-static
      nlohmann_json::nlohmann_json
  )
else()
  target_link_libraries(iotea_sdk_cpp
    PUBLIC
      paho-mqttpp3
      nlohmann_json::nlohmann_json
  )
endif()

target_compile_options(iotea_sdk_cpp PRIVATE -Wall -Wextra -pedantic -Werror)

include(GoogleTest)
if(BUILD_TESTING)
    add_subdirectory(mocks)

    add_executable(testrunner
        tests/test_iotea.cpp
        tests/test_schema.cpp
        tests/test_util.cpp
    )

    target_link_libraries(testrunner PRIVATE
        iotea_sdk_cpp
        iotea_sdk_cpp_mocks
        gtest
        gtest_main
        pthread
    )

   target_code_coverage(testrunner EXCLUDE tests/*)

    gtest_discover_tests(testrunner)
endif()

# installation
set(INSTALL_TARGETS
    iotea_sdk_cpp
)

## install binaries
install(TARGETS ${INSTALL_TARGETS} EXPORT IoTeaSdkCpp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
