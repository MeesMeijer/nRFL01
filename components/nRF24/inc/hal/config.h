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
#if defined(__CC_ARM) || defined(STM32F756xx)
#define USE_STM32
#elif defined(ESP_PLATFORM) 
#define USE_ESP_IDF
#else 
    /* Default to USE_ESP_IDF if no platform is defined */
    #define USE_ESP_IDF
#endif 

/**
 * @brief Toggle debug mode, this will print all read/write spi transactions.
 * 
 */
#define NRF24_DEBUG


#endif 
