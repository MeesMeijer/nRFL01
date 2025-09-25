# if(NOT IDF_PATH)
#     return()
# endif()

idf_component_register(
    SRCS "src/nRF24.c" "src/wrappers/spiw_idf.c" "src/wrappers/spiw_arduino.c" "src/wrappers/spiw_stm32.c"
    INCLUDE_DIRS .
    # REQUIRES 
)