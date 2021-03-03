#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(function_provider
    function_provider.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(function_provider PUBLIC iotea_sdk_cpp)
