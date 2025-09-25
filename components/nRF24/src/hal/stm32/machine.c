#include "inc/hal/config.h"
#include "inc/hal/machine.h"

#ifdef USE_STM32
#include "stm32f4xx_hal.h"
#include <stdlib.h>

// ================= SPI Handle ==================
struct spi_handle {
    SPI_HandleTypeDef *hspi;  // STM32 HAL SPI handle
    uint16_t cs_pin;
    GPIO_TypeDef *cs_port;
};

// ================= SPI Functions ==================

static spi_handle_t* spi_open(int bus, uint32_t freq_hz, int mode, int cs_pin) {
    (void)bus;  // STM32 usually has predefined SPI handles
    // Map bus 0 -> SPI1, bus 1 -> SPI2, etc.
    SPI_HandleTypeDef *hspi = NULL;
    if (bus == 0) hspi = &hspi1;
    else if (bus == 1) hspi = &hspi2;
    else return NULL;

    // Note: STM32 HAL SPI init must be done via CubeMX or HAL_SPI_Init()
    spi_handle_t *h = malloc(sizeof(spi_handle_t));
    h->hspi = hspi;

    // CS pin must be GPIO configured manually
    h->cs_port = GPIOA; // example, adjust per board
    h->cs_pin = cs_pin;
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
    return h;
}

static int spi_transmit(spi_handle_t *h, const uint8_t *data, size_t len, uint32_t timeout_ms) {
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
    int status = HAL_SPI_Transmit(h->hspi, (uint8_t*)data, len, timeout_ms);
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
    return status;
}

static int spi_receive(spi_handle_t *h, uint8_t *data, size_t len, uint32_t timeout_ms) {
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
    int status = HAL_SPI_Receive(h->hspi, data, len, timeout_ms);
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
    return status;
}

static int spi_transmit_receive(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len, uint32_t timeout_ms) {
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
    int status = HAL_SPI_TransmitReceive(h->hspi, (uint8_t*)tx, rx, len, timeout_ms);
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
    return status;
}

static void spi_close(spi_handle_t *h) {
    if (h) free(h);
}

// ================= GPIO ==================
static int gpio_config(int pin, bool output) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_TypeDef *port = GPIOA; // example mapping, adjust for your board
    GPIO_InitStruct.Pin = 1 << (pin % 16); // simple mapping, adjust if needed
    GPIO_InitStruct.Mode = output ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    return 0;
}

static int gpio_write(int pin, bool level) {
    GPIO_TypeDef *port = GPIOA; // same mapping as gpio_config
    HAL_GPIO_WritePin(port, 1 << (pin % 16), level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

static int gpio_read(int pin) {
    GPIO_TypeDef *port = GPIOA; // same mapping
    return HAL_GPIO_ReadPin(port, 1 << (pin % 16));
}

// ================= Sleep ==================
static void sleep_ms(uint32_t ms) {
    HAL_Delay(ms);
}

// ================= Machine Assembly ==================
static machine_t stm32_machine = {
    .spi   = { spi_open, spi_transmit, spi_receive, spi_transmit_receive, spi_close },
    .gpio  = { gpio_config, gpio_write, gpio_read },
    .sleep = { sleep_ms }
};

extern void hal_init(machine_t *machine){
    *machine = stm32_machine;
}

#endif 