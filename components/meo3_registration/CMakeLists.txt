idf_component_register(
    SRCS "Meo3_Registration.cpp"
    INCLUDE_DIRS "."
    REQUIRES meo3_type json esp_wifi nvs_flash lwip
)