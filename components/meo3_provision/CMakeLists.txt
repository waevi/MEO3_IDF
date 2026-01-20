idf_component_register(SRCS "Meo3_BleProvision.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__arduino-esp32 bt meo3_type meo3_storage meo3_ble)
