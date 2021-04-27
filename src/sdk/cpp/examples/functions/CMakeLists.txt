#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(functions
    functions.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(functions PUBLIC iotea_sdk_cpp)
