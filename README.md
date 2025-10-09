# nRFL01

> **Note:** This project is heavily based on [nRF24/RF24](https://github.com/nRF24/RF24).

⚠️ **Warning:** This project is still in an early stage. Features and documentation may change frequently, use with caution.


## Motivation / Goal. 
After using the [nRF24 project](https://github.com/nRF24/RF24) a long time, I wanted more flexibility in the code portability. Therefore I created this version of the nRF24 driver that focuses on being supported on a lot of platforms.  

# Short Goal
To create a NRF24 lib that is easily ported or "wrapped" around different targets by using c/c++ General API's with function pointers. This means that the nrf24.c is clean and logic only, and can be tested by unit tests. 

## Todo:
- [x] Finish esp-idf implementation
- [x] Add STM32 HAL implementation
- [ ] Add Linux/ RPI/ Luckfox HAL implementation
- [ ] Create documentation (Doxygen)

