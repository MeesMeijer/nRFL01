#include "inc/config.h"

/* Created for ESP-IDF V5.5.1 */

/* Only include if wrapper selected. */
#ifdef USE_ESP_IDF

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "inc/spiw.h"

spiw_t spwi_init(spiw_t *bus, const spiw_cfg_t *cfg) {
    
    return *bus;
}; 




#endif 