
#include "stdio.h"
#include "inc/nRF24.h"
#include "freertos/Freertos.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
/* 
    Current setup: 
    RF24: 
    Name  | Pin 
    SCK:  D18
    MOSI: D23
    MISO: D19
    CE:   D4
    CSN:  D5
    IRQ:  D34

    I2S: 
    LCK: D25 
    DIN: D32
    BCK: D26
    SCK: -1 

    UART: 
    TX:  D16
    RX:  D17


    Audio -> UART (ESP) -> SPI3 (NRF24) _-_-_-_-_ (NRF24) -> SPI3 (ESP) -> I2S -> TCM5012A -> SPEAKER 
*/


nrf24_cfg_t config = {
    .payloadSize  = 32,
    .channel      = 100,
    .crc          = NRF24_CRC_8,
    .datarate     = NRF24_1MBPS,
    .addressWidth = 5    
};

extern const machine_t *machine; 
static spi_handle_t *_spi2;  

void app_main(void){
    bool isConnected = false;

    hal_init(&machine);

    _spi2 = machine->spi.open(2, 1*1000*1000, 0);

    nrf24_status_t stat = nRF24_init(_spi2, &config);
    
    printf("nRF24_Init returned: %d\n", stat);

    vTaskDelay(15);

    while (true){
        (void)nRF24_isConnected(&isConnected);
        printf("Is avalible %d\n", isConnected);
        vTaskDelay(1000);
    }
   
};