cmake_minimum_required(VERSION 3.10)

set(CMAKE_PROJECT_NAME basic-functions)


# Link the Cross-Compiler
set(CMAKE_C_COMPILER ${CMAKE_SOURCE_DIR}/../sdk/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc)

# Set the NRF24 Lib
set(USE_LINUX_LUCKFOX ON)
add_compile_definitions(USE_LINUX_LUCKFOX)
set(NRF24_LIB_COMPONENT ${CMAKE_SOURCE_DIR}/../../../../nRF24)


project(${CMAKE_PROJECT_NAME})
add_executable(${CMAKE_PROJECT_NAME} main.c)
add_subdirectory(${NRF24_LIB_COMPONENT}  ${CMAKE_BINARY_DIR}/nRF24)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    
    # Add user defined libraries
    nRF24
)