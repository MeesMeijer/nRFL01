#include "inc/hal/config.h"
#include "inc/hal/machine.h"

#ifdef USE_ARDUINO

#include <Arduino.h>
#include <SPI.h>

struct spi_handle {
    SPIClass *spi;
    int cs_pin;
};

static spi_handle_t* spi_open(int bus, uint32_t freq_hz, int mode, int cs_pin) {
    (void)bus; // Arduino usually has only one SPI
    SPI.begin();
    SPI.beginTransaction(SPISettings(freq_hz, MSBFIRST, mode));
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    spi_handle_t *h = (spi_handle_t*)malloc(sizeof(spi_handle_t));
    h->spi = &SPI;
    h->cs_pin = cs_pin;
    return h;
}

static int spi_transmit(spi_handle_t *h, const uint8_t *data, size_t len, uint32_t timeout_ms) {
    digitalWrite(h->cs_pin, LOW);
    for (size_t i = 0; i < len; i++) h->spi->transfer(data[i]);
    digitalWrite(h->cs_pin, HIGH);
    return 0;
}

static int spi_receive(spi_handle_t *h, uint8_t *data, size_t len, uint32_t timeout_ms) {
    digitalWrite(h->cs_pin, LOW);
    for (size_t i = 0; i < len; i++) data[i] = h->spi->transfer(0xFF);
    digitalWrite(h->cs_pin, HIGH);
    return 0;
}

static int spi_transmit_receive(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len, uint32_t timeout_ms) {
    digitalWrite(h->cs_pin, LOW);
    for (size_t i = 0; i < len; i++) rx[i] = h->spi->transfer(tx[i]);
    digitalWrite(h->cs_pin, HIGH);
    return 0;
}

static void spi_close(spi_handle_t *h) {
    if (h) {
        digitalWrite(h->cs_pin, HIGH);
        free(h);
    }
}

// GPIO
static int gpio_config(int pin, bool output) {
    pinMode(pin, output ? OUTPUT : INPUT);
    return 0;
}

static int gpio_write(int pin, bool level) {
    digitalWrite(pin, level);
    return 0;
}

static int gpio_read(int pin) {
    return digitalRead(pin);
}

// Sleep
static void sleep_ms(uint32_t ms) {
    delay(ms);
}


static machine_t arduino_machine = {
    .spi   = { spi_open, spi_transmit, spi_receive, spi_transmit_receive, spi_close },
    .gpio  = { gpio_config, gpio_write, gpio_read },
    .sleep = { sleep_ms }
};

extern void nRF24_halInit(machine_t *machine){
    *machine = arduino_machine;
}

#endif 