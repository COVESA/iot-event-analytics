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

find_library(LIBDL dl)

add_library(iotea_sdk_cpp
  SHARED
    src/call.cpp
    src/client.cpp
    src/context.cpp
    src/event.cpp
    src/jsonquery.cpp
    src/logging.cpp
    src/protocol_gateway.cpp
    src/schema.cpp
    src/talent.cpp
    src/testsuite_talent.cpp
    src/util.cpp
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
      ${LIBDL}
  )
else()
  target_link_libraries(iotea_sdk_cpp
    PUBLIC
      paho-mqttpp3
      nlohmann_json::nlohmann_json
      ${LIBDL}
  )
endif()

target_compile_options(iotea_sdk_cpp PRIVATE -Wall -Wextra -pedantic -Werror)

if(BUILD_TESTING)

    # Google Test
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        release-1.10.0
    )

    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

    # enable testing
    include(CTest)
    include(GoogleTest)

    add_executable(testrunner
        tests/test_call.cpp
        tests/test_client.cpp
        tests/test_context.cpp
        tests/test_event.cpp
        tests/test_jsonquery.cpp
        tests/test_protocol_gateway.cpp
        tests/test_schema.cpp
        tests/test_talent.cpp
        tests/test_testsuite_talent.cpp
        tests/test_util.cpp
    )

    target_link_libraries(testrunner PRIVATE
        iotea_sdk_cpp
        gtest
        gmock
        gtest_main
        pthread
        dl
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

