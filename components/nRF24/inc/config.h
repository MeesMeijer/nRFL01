#ifndef CONFIG_H
#define CONFIG_H

/* Define the wrapper to use.  */
// #define USE_ESP_IDF
#if defined(__CC_ARM)
#define USE_STM32
#elif defined(ARDUINO)
#define USE_ARDUINO
#elif defined(ESP_PLATFORM) 
#define USE_ESP_IDF
// #else 
// #error "Unsupported Platform."
#endif 



#endif 