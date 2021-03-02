add_library(iotea_test_cpp SHARED src/integrationtest.cpp
)

## link binaries
target_include_directories(iotea_test_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(iotea_test_cpp PUBLIC iotea_sdk_cpp)

add_subdirectory("../../../../test/integration-tests/testset_sdk/cpp" "${CMAKE_CURRENT_BINARY_DIR}/testset_sdk")
