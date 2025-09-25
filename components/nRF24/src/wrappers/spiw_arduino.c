#include "inc/config.h"

#ifdef USE_ARDUINO

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "inc/spiw.h"

spiw_t spwi_init(spiw_t *bus, const spiw_cfg_t *cfg) {

    return *bus; 
}; 



#endif 