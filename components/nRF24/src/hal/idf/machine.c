#include "inc/hal/config.h"
#include "inc/hal/machine.h"

#ifdef USE_ESP_IDF
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

struct spi_handle {
    spi_device_handle_t dev;
};

static spi_handle_t* spi_open(uint8_t bus, uint32_t freq_hz, uint8_t mode) {
    if (bus >= SPI_HOST_MAX){
        return NULL;
    }

    if (mode > 3){
        return NULL; 
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = 19,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num =  -1,
        .data5_io_num =  -1,
        .data6_io_num =  -1,
        .data7_io_num =  -1,
        .max_transfer_sz = 256,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = freq_hz,
        .mode = mode,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    /* Because the spi messages are going to be short. Disable DMA to limit Queue overhead */
    ESP_ERROR_CHECK(spi_bus_initialize(bus, &buscfg, SPI_DMA_DISABLED));

    spi_handle_t *h = malloc(sizeof(spi_handle_t));
    ESP_ERROR_CHECK(spi_bus_add_device(bus, &devcfg, &h->dev));
    return h;
}

static int spi_begin_transaction(spi_handle_t *h){
    esp_err_t error = spi_device_acquire_bus(h->dev, portMAX_DELAY);
    ESP_ERROR_CHECK(error);
    return error;  
}

static void spi_end_transaction(spi_handle_t *h){
    (void)spi_device_release_bus(h->dev);
}

static int spi_write(spi_handle_t *h, const uint8_t *data, size_t len) {
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data, .rx_buffer = NULL};
    esp_err_t error = spi_device_transmit(h->dev, &t);
    ESP_ERROR_CHECK(error);
    return error;  
}

static int spi_read(spi_handle_t *h, uint8_t *data, size_t len) {
    spi_transaction_t t = { .length = len * 8, .tx_buffer = NULL, .rx_buffer = data };
    esp_err_t error = spi_device_transmit(h->dev, &t);
    ESP_ERROR_CHECK(error);
    return error;  
}

static int spi_transfer(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len) {
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    esp_err_t error = spi_device_transmit(h->dev, &t);
    ESP_ERROR_CHECK(error);
    return error;  
}

static void spi_close(spi_handle_t *h) {
    if (h != NULL) {
        spi_bus_remove_device(h->dev);
        free(h);
    }
}

// GPIO
static int gpio_configure(int pin, bool output) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT
    };
    return gpio_config(&io_conf);
}

static int gpio_write(int pin, bool level) {
    return gpio_set_level(pin, level);
}

static bool gpio_read(int pin) {
    return (bool)gpio_get_level(pin);
}

// Sleep
static void sleep_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static uint32_t millis(){
    return (uint32_t)(esp_timer_get_time() / 1000L);
}

// Assembled machine
static const machine_t idf_machine = {
    .spi   = { 
        .open       = spi_open, 
        .write   = spi_write, 
        .beginTransaction = spi_begin_transaction,
        .endTransaction   = spi_end_transaction,
        .read    =  spi_read, 
        .transfer   = spi_transfer, 
        .close      = spi_close 
    },
    .gpio  = { 
        .config = gpio_configure, 
        .write  = gpio_write, 
        .read   = gpio_read 
    },
    .sleep = { 
        .ms     = sleep_ms,
        .millis = millis
    }
};

const machine_t *machine = NULL;   // <-- actual definition

extern void nRF24_halInit(const machine_t **machine){
    if (machine == NULL){
        return; 
    }

    *machine = &idf_machine;
}


#endif 