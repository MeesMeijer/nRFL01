
# Decide which platform to build for. 

if(USE_ESP_IDF)
    idf_component_register(
        SRCS 
            "src/nRF24.c" 
            "src/hal/idf/machine.c"
        INCLUDE_DIRS 
            .
        PRIV_REQUIRES 
            esp_driver_spi esp_driver_gpio esp_timer esp_driver_gptimer
    )

elseif(USE_STM32)
    add_library(nRF24 STATIC
        src/nRF24.c
        src/hal/stm32/machine.c  
    )

    target_link_libraries(nRF24 PRIVATE
        stm32cubemx
    )

elseif(USE_LINUX_LUCKFOX)
    add_library(nRF24 STATIC
        src/nRF24.c
        src/hal/linux-luckfox/machine.c
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
        USE_LINUX_LUCKFOX
    )


else()
    message(FATAL_ERROR "No supported platform selected. Define one of: ESP_PLATFORM, USE_STM32, LUCKFOX")
endif()

target_include_directories(nRF24 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)