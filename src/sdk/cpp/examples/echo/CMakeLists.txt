add_executable(echo_provider
    echo_provider.cpp
)
target_link_libraries(echo_provider PUBLIC iotea_sdk_cpp ${OS_SPECIFIC_LIBS})

add_executable(echo_consumer
    echo_consumer.cpp
)
target_link_libraries(echo_consumer PUBLIC iotea_sdk_cpp ${OS_SPECIFIC_LIBS})

add_executable(echo_observer
    echo_observer.cpp
)
target_link_libraries(echo_observer PUBLIC iotea_sdk_cpp ${OS_SPECIFIC_LIBS})
