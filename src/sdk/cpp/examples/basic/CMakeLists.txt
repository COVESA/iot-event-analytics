#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(basic
    basic.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(basic PUBLIC iotea_sdk_cpp)
