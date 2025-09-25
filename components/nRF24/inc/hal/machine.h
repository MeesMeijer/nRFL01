#ifndef MACHINE_HAL_H
#define MACHINE_HAL_H

#include <stdint.h>
#include <stddef.h>
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

// ====================== SPI ======================
typedef struct spi_handle spi_handle_t; // forward declare
typedef struct {
    // @brief Malloc and start the bus
    spi_handle_t* (*open)(int bus, uint32_t freq_hz, int mode);
    
    // @brief 
    int (*beginTransaction)(spi_handle_t *h);
    void (*endTransaction)(spi_handle_t *h);
    
    // @brief Transmit n bytes 
    int (*transmit)(spi_handle_t *h, const uint8_t *data, size_t len, uint32_t timeout_ms);
    // @brief Receive n bytes 
    int (*receive)(spi_handle_t *h, uint8_t *data, size_t len, uint32_t timeout_ms);
    // @brief Transfer and receive n bytes 
    int (*transfer)(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len, uint32_t timeout_ms);
    // @brief Close and free the bus. 
    void (*close)(spi_handle_t *h);
} HAL_SPI_t;

// ====================== GPIO ======================

#define LOW 0
#define HIGH 1

typedef struct {
    int (*config)(int pin, bool output);
    int (*write)(int pin, bool level);
    int (*read)(int pin);
} HAL_GPIO_t;

// ====================== Sleep ======================
typedef struct {
    void (*ms)(uint32_t ms);
} HAL_Sleep_t;

// ====================== Machine Root ======================
typedef struct {
    HAL_SPI_t   spi;
    HAL_GPIO_t  gpio;
    HAL_Sleep_t sleep;
} machine_t;

extern const machine_t *machine;

extern void hal_init(const machine_t **machine);

#ifdef __cplusplus
}
#endif

#endif // MACHINE_HAL_H
