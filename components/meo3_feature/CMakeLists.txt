idf_component_register(SRCS "Meo3_Feature.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__arduino-esp32 mqtt json meo3_mqtt
                    )