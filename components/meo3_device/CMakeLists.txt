idf_component_register(SRCS "Meo3_Device.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__arduino-esp32 meo3_type meo3_storage meo3_provision meo3_ble meo3_mqtt
                    )