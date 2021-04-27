#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(callback_mode
    callback_mode.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(callback_mode PUBLIC iotea_sdk_cpp)
