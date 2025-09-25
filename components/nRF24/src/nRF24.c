#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "inc/nRF24.h"

nrf24_t nRF24_Init(nrf24_t *device, const nrf24_cfg_t *cfg){

    return *device;
};


nrf24_status_t nRF24_write(nrf24_t *device, const uint8_t *buffer, size_t length) {
    nrf24_status_t stat = NRF24_OK; 

    return stat; 
};


nrf24_status_t nRF24_fastWrite(nrf24_t *device, const uint8_t *buffer, size_t length) {
    nrf24_status_t stat = NRF24_OK; 

    return stat;
}; 


nrf24_status_t nRF24_avalible(nrf24_t *device) {
    nrf24_status_t stat = NRF24_OK; 

    return stat;
}; 


nrf24_status_t nRF24_read(nrf24_t *device, uint8_t *buffer, size_t length) {
    nrf24_status_t stat = NRF24_OK; 

    return stat;
};
