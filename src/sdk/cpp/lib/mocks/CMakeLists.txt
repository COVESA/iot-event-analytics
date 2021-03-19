## cmake flags
cmake_minimum_required(VERSION 3.10)

## project name
project("iotea-sdk-cpp-mocks-lib"
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

add_library(iotea_sdk_cpp_mocks INTERFACE)

## link binaries
target_include_directories(iotea_sdk_cpp_mocks
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(iotea_sdk_cpp_mocks
    INTERFACE
    iotea_sdk_cpp
    gmock
)

target_compile_options(iotea_sdk_cpp_mocks INTERFACE -Wall -Wextra -pedantic -Werror)

# installation
set(INSTALL_TARGETS
    iotea_sdk_cpp_mocks
)
