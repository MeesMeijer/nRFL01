
#include "stdio.h"
#include "stdlib.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "freertos/Freertos.h"
#include "freertos/task.h"

#include "inc/nRF24.h"

/* 
    Current setup: 
    RF24:  
        SCK:  D18
        MOSI: D23
        MISO: D19
        CE:   D4
        CSN:  D5
        IRQ:  D34 - Not used in this example. 
*/

#define CSN_PIN     5u
#define CE_PIN      4u

#define SPI_SPEED   10 *1000 *1000
#define SPI_PORT    2u 
#define SPI_MODE    0u

#define SIZE NRF24_MAX_PAYLOAD_SIZE

/* Addresses, should be same width/length as the nRF24_setAddressWidth() */
uint8_t address[][6] = { "1Node", "2Node" };
nrf24_cfg_t config = NRF24_DEFAULT_CFG(CSN_PIN, CE_PIN);

/* Use the extern *machine pointer.  */
extern const machine_t *machine; 
static spi_handle_t *_spi2;  

/* Determine if the current node is master or not. */
bool master      = false; 
bool isConnected = false;

uint32_t lastShown = 0;
uint32_t totalBytes = 0;
uint32_t lastBytes = 0;

void mainTask(void *arg);
void statsTask(void *arg);

void app_main(void){
    /* Wait for the chip to settle.  */
    (void)vTaskDelay(15u);
    
    /* Always init hal first.. else you have a nullptr error */
    (void)nRF24_halInit(&machine);

    /* Open the SPI controller.  */
    _spi2 = machine->spi.open(SPI_PORT, SPI_SPEED, SPI_MODE);
    machine->gpio.config(22u, false);

    /* Initialize the chip, and set the set configuration. */
    if (nRF24_init(_spi2, &config) != NRF24_OK){
        printf("[main] - nRF24_init failed. \r\n");
        for (;;) {
            /* Loop forever.. */
            printf("[main] - Hardware is not responding.. \r\n");
            (void)vTaskDelay(1000u);
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
            (void)vTaskDelay(1000u);
        }
    }

    /* Check if pin 22 is high, high == Master, low == Slave. */
    if (machine->gpio.read(22u)){
        master = true; 
    }

    /* Demo - Set other configurations */
    (void)nRF24_setDataRate(NRF24_1MBPS);
    (void)nRF24_setAddressWidth(5u);
    (void)nRF24_setPALevel(NRF24_PA_MIN, true);
    (void)nRF24_setAutoAck(false);
    (void)nRF24_setCrcLength(NRF24_CRC_8);

    (void)nRF24_setChannel(100u);
    (void)nRF24_setPayloadSize(SIZE);

    /* Open writing and reading pipes. */
    (void)nRF24_openWritingPipe(address[!master]);
    (void)nRF24_stopListening();
    (void)nRF24_openReadingPipe(1, (const uint8_t *)address[master]);

    xTaskCreate(mainTask, "mainTask", 4096, NULL, 1, NULL);
    xTaskCreate(statsTask, "statsTask", 4096, NULL, 2, NULL);
};

void mainTask(void *arg){

    TickType_t lastTick = xTaskGetTickCount();
    double accumulatedBytes = 0;

    if (!master){
        (void)nRF24_startListening(); 
        (void)nRF24_flushRx();

        uint8_t *buffer = (uint8_t *)malloc((sizeof(uint8_t) * SIZE)); 
        for (;;){
            if (nRF24_available()){
                (void)nRF24_read(buffer, SIZE);
                (void)printf("Received %d bytes. \n\t:", SIZE);
                for (int i = 0; i < 10; i++) {
                    printf("%02X ", buffer[i]);
                }
                (void)printf("\n");
            }
            /* Read as fast as possible, but wait 2 ticks for the WDT. */
            (void)vTaskDelay(2u);
        }
    }
    else {
        (void)nRF24_flushTx();

        uint8_t  buff_item = 0u;
        uint8_t *buffer    = (uint8_t *)malloc(sizeof(uint8_t) * SIZE);

        for (;;){
            TickType_t now = xTaskGetTickCount();
            accumulatedBytes += SIZE;

            /* Calculate the needed delay to achive a bytes stream of 44100 bytes/second */
            double elapsedSec = (now - lastTick) * (1.0 / configTICK_RATE_HZ);
            double expectedTime = accumulatedBytes / 44100u;
            if (expectedTime > elapsedSec) {
                double delaySec = expectedTime - elapsedSec;
                TickType_t delayTicks = (TickType_t)(delaySec * configTICK_RATE_HZ);
                if (delayTicks > 0) {
                    vTaskDelay(delayTicks);
                }
            }

            /* Reset after 1 second, prevent buff-overflow */
            now = xTaskGetTickCount();
            elapsedSec = (now - lastTick) * (1.0 / configTICK_RATE_HZ);
            if (elapsedSec >= 1.0) {
                lastTick = now;
                accumulatedBytes = 0;
            }

            /* Create a stream of 32 bytes that loop through 0-250 */
            // for (int i = 0; i < SIZE; i++){
            //     buffer[i] = buff_item;
            // }
            (void)memset(&buffer, buff_item, SIZE);
            buff_item = (buff_item + 1u) % 250u; 
            
            (void)nRF24_fastWrite(buffer, SIZE, false);
            totalBytes += 32u; 
        }
    }
}

void statsTask(void *arg) {
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));

        uint32_t diff = (totalBytes - lastBytes);
        lastBytes = totalBytes;

        printf("[Stats] Data rate: %lu bytes/s (total: %lu)\r\n", (unsigned long)diff, (unsigned long)totalBytes);
    }
}