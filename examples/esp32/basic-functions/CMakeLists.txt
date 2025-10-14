
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "./../../../components/nRF24") 

set(USE_ESP_IDF ON)
add_compile_definitions(USE_ESP_IDF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(nrf24)

