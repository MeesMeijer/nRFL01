#ifndef MACHINE_HAL_H
#define MACHINE_HAL_H

#include <stdint.h>
#include <stddef.h>
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spi_handle spi_handle_t;
typedef struct {
    // @brief Malloc and start the bus
    spi_handle_t* (*open)(uint8_t bus, uint32_t freq_hz, uint8_t mode);
    
    // @brief 
    int (*beginTransaction)(spi_handle_t *h);
    void (*endTransaction)(spi_handle_t *h);
    
    // @brief Transmit n bytes 
    int (*write)(spi_handle_t *h, const uint8_t *data, size_t len);
    // @brief Receive n bytes 
    int (*read)(spi_handle_t *h, uint8_t *data, size_t len);
    // @brief Transfer and receive n bytes 
    int (*transfer)(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len);
    // @brief Close and free the bus. 
    void (*close)(spi_handle_t *h);
} HAL_SPI_t;


#define LOW (uint8_t)0u
#define HIGH (uint8_t)1u

typedef struct {
    int (*config)(uint8_t pin, bool output);
    int (*write)(uint8_t pin, bool level);
    bool (*read)(uint8_t pin);
} HAL_GPIO_t;

typedef struct {
    void (*ms)(uint32_t ms);
    void (*us)(uint32_t us);
} HAL_Sleep_t;

typedef struct {
    uint32_t(*millis)(void);
} HAL_Time_t; 

typedef struct {
    HAL_SPI_t   spi;
    HAL_GPIO_t  gpio;
    HAL_Sleep_t sleep;
    HAL_Time_t  time; 
} machine_t;

extern const machine_t *machine;

extern void nRF24_halInit(const machine_t **machine);

#ifdef __cplusplus
}
#endif

#endif // MACHINE_HAL_H
