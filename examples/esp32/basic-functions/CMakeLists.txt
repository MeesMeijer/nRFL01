
cmake_minimum_required(VERSION 3.5)


set(CMAKE_ESP_IDF ON)
add_compile_definitions(CMAKE_ESP_IDF)

set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/../../../nRF24) 


include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(nrf24)

