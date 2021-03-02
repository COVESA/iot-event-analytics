add_executable(function_provider_for_test
    function_provider.cpp
)

target_include_directories(function_provider_for_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(function_provider_for_test PUBLIC iotea_sdk_cpp)

add_executable(testset_rpc
    testset_rpc.cpp
)

target_include_directories(testset_rpc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(testset_rpc
    PUBLIC
    iotea_test_cpp
    nlohmann_json::nlohmann_json)