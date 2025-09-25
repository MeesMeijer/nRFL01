#ifndef NRF24_H
#define NRF24_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "inc/spiw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int test; 
} nrf24_cfg_t;  

typedef struct {
    nrf24_cfg_t *cfg; 
    spiw_t      *spiw;
} nrf24_t; 

typedef enum {
    NRF24_OK, 
    NRF24_ERROR, 
    NRF24_NULL_POINTER,
    NRF24_ARG_INVALID
} nrf24_status_t; 

nrf24_t nRF24_Init(nrf24_t *device, const nrf24_cfg_t *cfg);

nrf24_status_t nRF24_write(nrf24_t *device, const uint8_t *buffer, size_t length);
nrf24_status_t nRF24_fastWrite(nrf24_t *device, const uint8_t *buffer, size_t length);

nrf24_status_t nRF24_avalible(nrf24_t *device);
nrf24_status_t nRF24_read(nrf24_t *device, uint8_t *buffer, size_t length);

#ifdef __cplusplus
};
#endif
#endif 