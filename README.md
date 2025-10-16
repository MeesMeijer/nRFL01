# nRFL01
A simple, and portable nRF24 driver that is fast and has great support for different platforms like ESP-IDF, STM32, Arduino and Linux.   
> **Note/Warning** This project is heavily based on [nRF24/RF24](https://github.com/nRF24/RF24), and is in its early stage of development. Features or implementations can change without warning. 

## Motivation / Goal. 
The motivation of this project is related to my frustrations about the current avaliable nRF24 drivers. After using the "official" [nRF24 project](https://github.com/nRF24/RF24) a long time i decided on forking / creating my own version, one that has a dedication to be more readable by hiding the platform specific code in hal wrappers. 

TL; DR; To create a nRF24 library that is easily ported or "wrapped" around different platforms by using general known c/c++ pointer api's. This means that the nrf24.c is clean and logic only, and can easily be tested.  

## "Proof of Concept" Roadmap:
- [x] Finish esp-idf implementation.
- [x] Add STM32 HAL implementation
- [x] Add Linux / RPI / Luckfox HAL implementation
- [ ] Add Arduino HAL implementation (Halted)
- [ ] Create documentation (Doxygen)

## "First Release" Roadmap:
- [ ] Check all platform dependencies.
- [ ] TBD.
- [ ] Release Version 1.0

---
# How to use. 
The project is created around CMake, this is done to limit the need for end-users configuration. Inorder to compile the project, 3 lines need to be added to your root CMakelists.txt. 

## Esp Idf v5.*
````cmake
set(EXTRA_COMPONENT_DIRS <path-to-repo>/components/nRF24) 
set(USE_ESP_IDF ON)
add_compile_definitions(USE_ESP_IDF)
````

## STM32 
````cmake
add_subdirectory(<path-to-repo>/components/nRF24 ${CMAKE_BINARY_DIR}/nRF24)
set(USE_STM32 ON)
add_compile_definitions(USE_STM32)
````
## Luckfox
````cmake
add_subdirectory(<path-to-repo>/components/nRF24 ${CMAKE_BINARY_DIR}/nRF24)
set(USE_LINUX_LUCKFOX ON)
add_compile_definitions(USE_LINUX_LUCKFOX)
````


