idf_component_register(SRCS "Meo3_Mqtt.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES meo3_type esp_event mqtt
                    )
                   