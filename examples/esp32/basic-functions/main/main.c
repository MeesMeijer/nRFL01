
#include "stdio.h"


#include "freertos/Freertos.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "inc/nRF24.h"

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

#define CSN_PIN 5u
#define CE_PIN  4u

#define SPI_SPEED 10*1000*1000
#define SPI_PORT 2u

uint8_t address[][6] = { "1Node", "2Node" };
nrf24_cfg_t config = NRF24_DEFAULT_CFG(CSN_PIN, CE_PIN);

extern const machine_t *machine; 
static spi_handle_t *_spi2;  
volatile bool master = false; 

void app_main(void){
    bool isConnected = false;

    config.channel = 100u;
    config.datarate = NRF24_1MBPS;

    (void)vTaskDelay(15);
    
    /* Always init hal first.. else you have a nullptr error */
    (void)nRF24_halInit(&machine);

    _spi2 = machine->spi.open(SPI_PORT, SPI_SPEED, 0);
    machine->gpio.config(22, false);

    nrf24_status_t stat = nRF24_init(_spi2, &config);
    
    printf("[main] - nRF24_Init returned: %d\n", stat);
    
    (void)nRF24_isConnected(&isConnected);

    if (!isConnected){
        printf("[main] - isConnected: %d\n", isConnected);
        while (true){}
    }

    if (machine->gpio.read(22)){
        master = true; 
    }


    nRF24_setPALevel(NRF24_PA_MIN, false);
    nRF24_setAutoAck(false);
    nRF24_setChannel(100u);

    
    nRF24_openWritingPipe(address[!master]);
    nRF24_stopListening();

    nRF24_openReadingPipe(1, (const uint8_t *)address[master]);
    
    if (!master){
        nRF24_startListening(); 
        nRF24_flushRx();

        uint8_t *buffer = (uint8_t *)malloc(32); 
        for (;;){
            if (nRF24_available()){
                (void)nRF24_read(buffer, NRF24_MAX_PAYLOAD_SIZE);
                printf("Received %d bytes. \n\t:", NRF24_MAX_PAYLOAD_SIZE);
                for (int i = 0; i < 10; i++) {
                    printf("%02X ", buffer[i]);
                }
                printf("\n");
            }
            vTaskDelay(2);
        }
    }
    else{
        nRF24_flushTx();

        uint8_t a = 0u;
        uint8_t *buffer = (uint8_t *)malloc(32);
        
        for (;;){
            for (int i = 0; i< NRF24_MAX_PAYLOAD_SIZE; i++){
                buffer[i] = a;
            }
            
            a = (a + 1u) % 250u; 
            printf("Sending 32 bytes\n");
            nRF24_fastWrite(buffer, NRF24_MAX_PAYLOAD_SIZE, false); // todo
            vTaskDelay(2);
        }
    }
};