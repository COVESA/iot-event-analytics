## cmake flags
cmake_minimum_required(VERSION 3.10)

## project name
project("mqtt-protocol-adapter-lib"
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

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(LIBS_SYSTEM ws2_32)
elseif(UNIX)
    set(LIBS_SYSTEM c stdc++)
endif()

add_library(mqtt_protocol_adapter
  SHARED
    src/mqtt_adapter.cpp
)

## link binaries

target_include_directories(mqtt_protocol_adapter
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mqtt_protocol_adapter
PUBLIC
  iotea_sdk_cpp
)

target_compile_options(mqtt_protocol_adapter PRIVATE -Wall -Wextra -pedantic -Werror)

# installation
set(INSTALL_TARGETS
    mqtt_protocol_adapter
)

## install binaries
install(TARGETS ${INSTALL_TARGETS} EXPORT MqttProtocolAdapter
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
