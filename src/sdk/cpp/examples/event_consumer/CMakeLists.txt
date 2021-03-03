add_executable(event_consumer
    event_consumer.cpp
)

target_include_directories(event_consumer PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(event_consumer PUBLIC iotea_sdk_cpp)
