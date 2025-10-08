#include "inc/hal/config.h"
#include "inc/hal/machine.h"


#ifdef USE_STM32

#include "main.h"
#include "stdlib.h"

#if defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#endif 


extern SPI_HandleTypeDef hspiX; 
extern TIM_HandleTypeDef htimXSleep;

/* Map all the gpio ports for easy passing */
#define MAX_PINS_PER_PORT (16u)
static GPIO_TypeDef *gpio_ports[] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH, GPIOI, GPIOJ, GPIOK
};

struct spi_handle {
    SPI_HandleTypeDef *hspi;  // STM32 HAL SPI handle
};

static spi_handle_t* spi_open(uint8_t bus, uint32_t freq_hz, uint8_t mode) {
    (void)bus;  /* Ignore the bus id, must be defined within Inc/main.h */

    SPI_HandleTypeDef *hspi = &hspiX;

    /* Note: STM32 HAL SPI init must be done via CubeMX or HAL_SPI_Init() */
    spi_handle_t *h = malloc(sizeof(spi_handle_t));
    h->hspi = hspi;

    return h;
}

static int spi_begin_transmission(spi_handle_t *h){
    /* Nothing to do, CE and CSN is Handle in lib */
    return 0; 
}

static void spi_end_transmission(spi_handle_t *h){
    /* Nothing to do, CE and CSN is Handle in lib */
}

static int spi_transmit(spi_handle_t *h, const uint8_t *data, size_t len) {
    int status = HAL_SPI_Transmit(h->hspi, (uint8_t*)data, len, 1000);
    return status;
}

static int spi_receive(spi_handle_t *h, uint8_t *data, size_t len) {
    int status = HAL_SPI_Receive(h->hspi, data, len, 1000);
    return status;
}

static int spi_transmit_receive(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len) {
    int status = HAL_SPI_TransmitReceive(h->hspi, (uint8_t*)tx, rx, len, 1000);
    return status;
}

static void spi_close(spi_handle_t *h) {
    if (h) free(h);
}

// ================= GPIO ==================
static int gpio_config(uint8_t pin, bool output) {
    /* Because stm32 uses ports and pins, decontruct the pin uint8_t where
        the first 4 bits represent the pin and the last 4 bits represent the port.  
    */

    /* Should we configure things?? or should we only configure things with CubeMX?? */
    
    // if (STM_GPIO_DECODE_PORT(pin) > 11){
    //     return -1; 
    // }

    // GPIO_InitTypeDef GPIO_InitStruct = {0};
    // GPIO_TypeDef *port = gpio_ports[STM_GPIO_DECODE_PORT(pin)]; // example mapping, adjust for your board
    // GPIO_InitStruct.Pin = 1 << (STM_GPIO_DECODE_PIN(pin) % 16); // simple mapping, adjust if needed
    // GPIO_InitStruct.Mode = output ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    // GPIO_InitStruct.Pull = GPIO_NOPULL;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    // HAL_GPIO_Init(port, &GPIO_InitStruct);
    return 0;
}

static int gpio_write(uint8_t pin, bool level) {
    /* Because stm32 uses ports and pins, decontruct the pin uint8_t where
        the first 4 bits represent the pin and the last 4 bits represent the port.  
    */
    
    GPIO_TypeDef *port = gpio_ports[STM_GPIO_DECODE_PORT(pin)];
    HAL_GPIO_WritePin(port, 1 << (STM_GPIO_DECODE_PIN(pin) % MAX_PINS_PER_PORT), level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

static bool gpio_read(uint8_t pin) {
    /* Because stm32 uses ports and pins, decontruct the pin uint8_t where
        the first 4 bits represent the pin and the last 4 bits represent the port.  
    */

    GPIO_TypeDef *port = gpio_ports[STM_GPIO_DECODE_PORT(pin)];
    return HAL_GPIO_ReadPin(port, 1 << (STM_GPIO_DECODE_PIN(pin) % MAX_PINS_PER_PORT));
}

/* Begin: Machine->sleep  */
static void sleep_setup(void){
    htimxSleepRCCEnable();
    
    /* Get the clock speed of the APB1 BUS, make sure that the htimXSleep is on this bus!  */
    uint32_t pclk   = HAL_RCC_GetPCLK1Freq();
    uint32_t timclk = pclk;

    /*  */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1){
        timclk *= 2u;
    }

    /* Setup prescaler for 1MHZ 
        Formula: (APB1 Clock / Wanted Hz) - 1
    */
    uint32_t prescaler = ( (timclk / 1000000UL) - 1u );
    htimXSleep.Init.Prescaler     = prescaler; 
    htimXSleep.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htimXSleep.Init.Period        = 0xFFFFFFFF;
    htimXSleep.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    TIM_Base_SetConfig(htimXSleep.Instance, &htimXSleep.Init);
    HAL_TIM_Base_Start(&htimXSleep);
}
static void sleep_ms(uint32_t ms) {
    uint32_t start = __HAL_TIM_GET_COUNTER(&htimXSleep);
    while ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < (ms*1000u)){
        /* Idle.. */
    }
}
static void sleep_us(uint32_t us){
    uint32_t start = __HAL_TIM_GET_COUNTER(&htimXSleep);
    while ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < us){
        /* Idle.. */
    }
}
/* End: Machine->sleep  */

/* Begin: Machine->time  */
static uint32_t millis(void){
    return HAL_GetTick();
}
/* End: Machine->time */

static machine_t stm32_machine = {
    .spi   = { 
        .open       = spi_open, 
        .write      = spi_transmit, 
        .beginTransaction = spi_begin_transmission,
        .endTransaction   = spi_end_transmission,
        .read       =  spi_receive, 
        .transfer   = spi_transmit_receive, 
        .close      = spi_close 
    },
    .gpio  = { 
        .config = gpio_config, 
        .write  = gpio_write, 
        .read   = gpio_read,
    },
    .sleep = { 
        .ms     = sleep_ms,
        .us     = sleep_us,
    },
    .time = {
        .millis = millis,
    }
};

const machine_t *machine = NULL;   // <-- actual definition

extern void nRF24_halInit(const machine_t **machine){
    if (machine == NULL){
        return; 
    }

    /* Init the Hardware timers.  */
    (void)sleep_setup();

    
    *machine = &stm32_machine;

}

#endif 