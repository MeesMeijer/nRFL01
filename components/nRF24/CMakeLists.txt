
# Decide which platform to build for. 

if(ESP_PLATFORM)
idf_component_register(
    SRCS 
        "src/nRF24.c" 
        "src/hal/idf/machine.c"
    INCLUDE_DIRS 
        .
    PRIV_REQUIRES 
        esp_driver_spi esp_driver_gpio esp_timer esp_driver_gptimer
)
else()
add_library(nRF24 STATIC
    src/nRF24.c
    src/hal/stm32/machine.c
)

target_include_directories(nRF24 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nRF24 PRIVATE
    # Add dependencies here if needed, e.g. SPI, GPIO drivers
    stm32cubemx
)
endif()