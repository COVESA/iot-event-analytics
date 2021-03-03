if(PAHO_WITH_SSL)
    set(_PAHO_MQTT_C_LIB_NAME paho-mqtt3as)
    find_package(OpenSSL REQUIRED)
    get_target_property(PAHO_MQTT_C_INCLUDE_DIRS paho-mqtt3as INCLUDE_DIRECTORIES)
else()
    set(_PAHO_MQTT_C_LIB_NAME paho-mqtt3a)
    if(PAHO_BUILD_STATIC)
        get_target_property(PAHO_MQTT_C_INCLUDE_DIRS  paho-mqtt3a-static INCLUDE_DIRECTORIES)
    else()
        get_target_property(PAHO_MQTT_C_INCLUDE_DIRS  paho-mqtt3a INCLUDE_DIRECTORIES)
    endif()
endif()

if(PAHO_BUILD_STATIC)
    get_target_property(PAHO_MQTT_C_LIBRARY_PATH paho-mqtt3a-static BINARY_DIR)
else()
    get_target_property(PAHO_MQTT_C_LIBRARY_PATH paho-mqtt3a BINARY_DIR)
endif()

add_library(PahoMqttC::PahoMqttC IMPORTED SHARED)
set_target_properties(PahoMqttC::PahoMqttC PROPERTIES
    if(PAHO_BUILD_STATIC)
        IMPORTED_LOCATION "${PAHO_MQTT_C_LIBRARY_PATH}/libpaho-mqtt3a.a" # TODO: Make platform independent
    else()
        IMPORTED_LOCATION "${PAHO_MQTT_C_LIBRARY_PATH}/libpaho-mqtt3a.so" # TODO: Make platform independent
    endif()
    INTERFACE_INCLUDE_DIRECTORIES "${PAHO_MQTT_C_INCLUDE_DIRS}"
    #INCLUDE_DIRECTORIES "${PAHO_MQTT_C_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
)
if(PAHO_WITH_SSL)
    set_target_properties(PahoMqttC::PahoMqttC PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "OPENSSL=1"
        INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto"
        if(PAHO_BUILD_STATIC)
          IMPORTED_LOCATION "${PAHO_MQTT_C_LIBRARY_PATH}/libpaho-mqtt3as.a" # TODO: Make platform independent
        else()
          IMPORTED_LOCATION "${PAHO_MQTT_C_LIBRARY_PATH}/libpaho-mqtt3as.so" # TODO: Make platform independent
        endif()
        INTERFACE_INCLUDE_DIRECTORIES "${PAHO_MQTT_C_INCLUDE_DIRS}"
        #INCLUDE_DIRECTORIES "${PAHO_MQTT_C_INCLUDE_DIRS}"
    )
endif()

if(PAHO_BUILD_STATIC)
  add_dependencies(PahoMqttC::PahoMqttC paho-mqtt3a-static)
else()
  add_dependencies(PahoMqttC::PahoMqttC paho-mqtt3a)
endif()
