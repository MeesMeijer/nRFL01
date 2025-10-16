#ifndef CONFIG_H
#define CONFIG_H

/**
 * @brief Encode the port and pin into a uint8_t Byte. 
 * 
 * @param port: GPIOA = 0 -> 10 = GPIOK
 * @param pin: 0-15
 * 
 * @example uint8_t res = STM_GPIO_ENCODE(0, 5); // Example with pin number 5 and portA
 * 
 * */  
#define STM_GPIO_ENCODE(port, pin) (( (port) << 4u ) | (pin))
#define STM_GPIO_DECODE_PORT(x)    (((x) >> 4u) & 15u)
#define STM_GPIO_DECODE_PIN(x)     ((x) & 15u)


/* Define the wrapper to use.  */
#if defined(CMAKE_STM32)
#define USE_STM32
#elif defined(CMAKE_ESP_IDF)
#define USE_ESP_IDF
#elif defined(CMAKE_LUCKFOX)
#define USE_LINUX_LUCKFOX
#endif 

/**
 * @brief Toggle debug mode, this will print all read/write spi transactions.
 * 
 */
// #define NRF24_DEBUG


#endif 
