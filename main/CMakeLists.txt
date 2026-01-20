idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__arduino-esp32 meo3_device meo3_ble meo3_mqtt meo3_provision meo3_registration meo3_storage meo3_type meo3_feature)
