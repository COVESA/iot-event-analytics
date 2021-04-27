#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(subclass_mode
    subclass_mode.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(subclass_mode PUBLIC iotea_sdk_cpp)
