#ifndef SPI_WRAPPER_H
#define SPI_WRAPPER_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int miso; 
    int mosi; 
    int cs; 
    int sck; 
} spiw_cfg_t;

typedef struct {
    spiw_cfg_t *cfg; 
    void *handler; 
} spiw_t;


spiw_t spiw_init(spiw_t *bus, const spiw_cfg_t *cfg);

size_t spiw_read(spiw_t *bus);
size_t spiw_write(spiw_t *bus);

size_t spiw_transfer(spiw_t *bus);


#ifdef __cplusplus
};
#endif
#endif 