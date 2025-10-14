#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

#include "inc/nRF24.h"

static spi_handle_t     *_spi2;  
extern const machine_t  *machine;

nrf24_cfg_t config       = NRF24_DEFAULT_CFG(54, 55);
uint8_t     address[][6] = { "1Node", "2Node" }; /* A 5 byte wide node address. */
bool        isConnected  = false; 
bool        isMaster     = false;  /* Default to RX */

uint8_t  buffer[32u + 1u]; 
uint32_t lastShown  = 0;
uint32_t totalBytes = 0;
uint32_t lastBytes  = 0;


int main(){
    int      res    = 0;

    /* Init hal. */
    (void)nRF24_halInit(&machine);
    _spi2 = machine->spi.open(0, 10*1000*1000, 0);
    
    /* Init nrf24 */
    if ((res = nRF24_init(_spi2, &config)) != NRF24_OK){
        printf("[main] - nRF24_init failed. \r\n");
        for (;;) {
            /* Loop forever.. */
            printf("[main] - Hardware is not responding.. \r\n");
            machine->sleep.ms(1000);
        }
    }

    /* Check if Chip is connected.  */
    if (nRF24_isConnected(&isConnected) != NRF24_OK){
        printf("[main] - Could't .isConnected. \r\n");
    }
    else if (!isConnected){
        printf("[main] - isConnected: %d\n", isConnected);
        for (;;){
            /* Loop Forever */
            printf("[main] - Hardware is not responding.. \r\n");
            machine->sleep.ms(1000);
        }
    }

    /* Demo - Set other configurations */
    (void)nRF24_setDataRate(NRF24_1MBPS);
    (void)nRF24_setAddressWidth(5u);
    (void)nRF24_setPALevel(NRF24_PA_MIN, true);
    (void)nRF24_setAutoAck(false);
    (void)nRF24_setCrcLength(NRF24_CRC_8);

    (void)nRF24_setChannel(100u);
    (void)nRF24_setPayloadSize(32u);

    /* Open writing and reading pipes. */
    (void)nRF24_openWritingPipe(address[!isMaster]);
    (void)nRF24_stopListening();
    
    (void)nRF24_openReadingPipe(1, (const uint8_t *)address[isMaster]);
    
    (void)nRF24_startListening(); 
    (void)nRF24_flushRx();

    while (true) {

        /* Check if packets are avalible, and read them. */
        if (nRF24_available()){
            nRF24_read(&buffer, 32u);
            totalBytes += 32u; 
        }

        /* Print the received bytes/second every second. */
        if ((machine->time.millis() - lastShown) > 1e3){
            uint32_t diff = (totalBytes - lastBytes);
            lastBytes = totalBytes;
            
            printf("[Stats] Data rate: %lu bytes/s (total: %lu)\r\n", (unsigned long)diff, (unsigned long)totalBytes);

            lastShown = machine->time.millis();
        }
    }
}