
# Decide which platform to build for. 

if(ESP_PLATFORM)
idf_component_register(
    SRCS 
        "src/nRF24.c" 
        "src/hal/idf/machine.c"
    INCLUDE_DIRS 
        .
    PRIV_REQUIRES 
        esp_driver_spi esp_driver_gpio esp_timer
)
else()

endif()