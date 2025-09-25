#include "inc/hal/config.h"
#include "inc/hal/machine.h"

#ifdef USE_ESP_IDF
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct spi_handle {
    spi_device_handle_t dev;
};

static spi_handle_t* spi_open(int bus, uint32_t freq_hz, int mode) {
    if (bus >= SPI_HOST_MAX){
        return NULL;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = 19,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32 + 1,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = freq_hz,
        .mode = mode,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    static int initialized = 0;
    if (!initialized) {
        /* Because the spi messages are going to be short. Disable DMA to limit overhead */
        spi_bus_initialize(bus, &buscfg, SPI_DMA_DISABLED);
        initialized = 1;
    }

    spi_handle_t *h = malloc(sizeof(spi_handle_t));
    spi_bus_add_device(bus, &devcfg, &h->dev);
    return h;
}

static int spi_begin_transaction(spi_handle_t *h){
    return spi_device_acquire_bus(h->dev, portMAX_DELAY);
}

static void spi_end_transaction(spi_handle_t *h){
    return spi_device_release_bus(h->dev);
}

static int spi_transmit(spi_handle_t *h, const uint8_t *data, size_t len, uint32_t timeout_ms) {
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data };
    return spi_device_transmit(h->dev, &t);
}

static int spi_receive(spi_handle_t *h, uint8_t *data, size_t len, uint32_t timeout_ms) {
    spi_transaction_t t = { .length = len * 8, .rx_buffer = data };
    return spi_device_transmit(h->dev, &t);
}

static int spi_transmit_receive(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len, uint32_t timeout_ms) {
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    return spi_device_transmit(h->dev, &t);
}

static void spi_close(spi_handle_t *h) {
    if (h) {
        spi_bus_remove_device(h->dev);
        free(h);
    }
}

// GPIO
static int gpio_configure(int pin, bool output) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    return gpio_config(&io_conf);
}

static int gpio_write(int pin, bool level) {
    return gpio_set_level(pin, level);
}

static int gpio_read(int pin) {
    return gpio_get_level(pin);
}

// Sleep
static void sleep_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

    // Assembled machine
static const machine_t idf_machine = {
    .spi   = { 
        .open       = spi_open, 
        .transmit   = spi_transmit, 
        .beginTransaction = spi_begin_transaction,
        .endTransaction   = spi_end_transaction,
        .receive    =  spi_receive, 
        .transfer   = spi_transmit_receive, 
        .close      = spi_close 
    },
    .gpio  = { 
        gpio_configure, 
        gpio_write, 
        gpio_read 
    },
    .sleep = { 
        sleep_ms 
    }
};

const machine_t *machine = NULL;   // <-- actual definition

extern void hal_init(const machine_t **machine){
    if (machine == NULL){
        return; 
    }

    *machine = &idf_machine;
}


#endif 