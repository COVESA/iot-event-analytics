## cmake flags
cmake_minimum_required(VERSION 3.10)

## project name
project("iotea-testset-sdk-cpp"
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

set(IOTEA_SDK_CPP_PATH "../../../../src/sdk/cpp")

add_subdirectory(${IOTEA_SDK_CPP_PATH} ${CMAKE_BINARY_DIR}/iotea-sdk-cpp-lib)

message(${CMAKE_SOURCE_DIR}/${IOTEA_SDK_CPP_PATH}/lib/include)

add_executable(function_provider_for_test
    function_provider.cpp
)

target_include_directories(function_provider_for_test PRIVATE ${CMAKE_SOURCE_DIR}/${IOTEA_SDK_CPP_PATH}/lib/include)
target_link_libraries(function_provider_for_test PUBLIC iotea_sdk_cpp)

    
add_executable(testset_sdk
    testset_sdk.cpp
)

target_include_directories(testset_sdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(testset_sdk
    PUBLIC
    iotea_sdk_cpp
    mqtt_protocol_adapter
    nlohmann_json::nlohmann_json)
