#include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(basic
    basic.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/pgconfig.json
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

include_directories("${CMAKE_SOURCE_DIR}/include")
target_link_libraries(basic PUBLIC iotea_sdk_cpp)
