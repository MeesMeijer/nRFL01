#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"
#include "stdio.h"

#include "inc/hal/machine.h"

#include "inc/nRF24L01.h"
#include "inc/nRF24.h"

/* Private functions */
static nrf24_status_t _initRadio();
static nrf24_status_t _beginTransaction();
static nrf24_status_t _endTransaction();
static nrf24_status_t _toggleFeatures(void);

static nrf24_status_t _readRegister(uint8_t req, uint8_t *result);
static nrf24_status_t _readRegisternb(uint8_t req, uint8_t *buffer, size_t length);

static nrf24_status_t _writeRegister(uint8_t reg, const uint8_t value);
static nrf24_status_t _writeRegisternb(uint8_t reg, const uint8_t* buf, uint8_t length);


static void csn(bool level);
static void ce(bool level);


static spi_handle_t *_spi = NULL; 
static nrf24_cfg_t  *_cfg  = NULL;

static uint8_t *tx_buffer = NULL;
static uint8_t *rx_buffer = NULL;
static bool    _is_p_variant = false; 

static uint8_t config_reg;               /* For storing the value of the NRF_CONFIG register */

nrf24_status_t nRF24_init(spi_handle_t *spi, const nrf24_cfg_t *cfg){
    nrf24_status_t stat = NRF24_OK;

    if ((spi == NULL) || (cfg == NULL)) {
        stat = NRF24_NULL_POINTER;
    }
    else if (_spi != NULL){
        stat = NRF24_ERROR; 
    }
    else {
        /* Init the NRF24 */
        _spi = spi;
        _cfg = cfg; 

        machine->gpio.config(_cfg->gpio.ce, true);
        machine->gpio.config(_cfg->gpio.csn, true);
        
        ce(LOW);
        csn(HIGH);

        tx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 33);
        rx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 33);
        
        stat = _initRadio();
    }
    return stat; 
};


nrf24_status_t nRF24_isConnected(bool *connected) {
    uint8_t result = 0; 

    (void)_readRegister(SETUP_AW, &result);
    *connected = (result == (_cfg->addressWidth - (uint8_t)(2)));

    return NRF24_OK;
};

nrf24_status_t nRF24_powerUp(void){
    // if not powered up then power up and wait for the radio to initialize
    if (!(config_reg & _BV(PWR_UP))) {
        config_reg |= _BV(PWR_UP);
        _writeRegister(NRF_CONFIG, config_reg);

        // For nRF24L01+ to go from power down mode to TX or RX mode it must first pass through stand-by mode.
        // There must be a delay of Tpd2stby (see Table 16.) after the nRF24L01+ leaves power down mode before
        // the CEis set high. - Tpd2stby can be up to 5ms per the 1.0 datasheet
        machine->sleep.ms(5);
    }
    return NRF24_OK;
}

nrf24_status_t nRF24_powerDown(void){
    ce(LOW); // Guarantee CE is low on powerDown
    config_reg = (uint8_t)(config_reg & ~_BV(PWR_UP));
    _writeRegister(NRF_CONFIG, config_reg);
    return NRF24_OK;
}



/* Static Functions */

static nrf24_status_t _initRadio() {
    uint8_t status     = 0u; 

    /* Sleep 5 ms to allow the radio to settle. */
    machine->sleep.ms(5u);

    (void)_writeRegister(SETUP_RETR, (uint8_t)(rf24_min(15, 5) << ARD | rf24_min(15, 15)));

    (void)_writeRegister(NRF_STATUS, NRF24_IRQ_ALL);

    uint8_t before_toggle; 
    (void)_readRegister(FEATURE, &before_toggle);
    printf("[nRF24] _initRadio: FEATURE before toggle: 0x%02X\n", before_toggle);
   
    /* Toggle features */
    (void)_toggleFeatures();

    uint8_t after_toggle;
    (void)_readRegister(FEATURE, &after_toggle);
    printf("[nRF24] _initRadio: FEATURE after_toggle: 0x%02X\n", after_toggle);

    _is_p_variant = before_toggle == after_toggle;
    if (after_toggle) {
        if (_is_p_variant) {
            // module did not experience power-on-reset (#401)
            (void)_toggleFeatures();
        }
        // allow use of multicast parameter and dynamic payloads by default
        _writeRegister(FEATURE, 0);
    }
    // Clear CONFIG register:
    //      Reflect all IRQ events on IRQ pin
    //      Enable PTX
    //      Power Up
    //      16-bit CRC (CRC required by auto-ack)
    // Do not write CE high so radio will remain in standby I mode
    // PTX should use only 22uA of power
    (void)_writeRegister(NRF_CONFIG, (_BV(EN_CRC) | _BV(CRCO)));
    (void)_readRegister(NRF_CONFIG, &config_reg);

    (void)nRF24_powerUp();

    return (config_reg == (_BV(EN_CRC) | _BV(CRCO) | _BV(PWR_UP))) ? NRF24_OK : NRF24_ERROR;
}

static nrf24_status_t _toggleFeatures(void){
    uint8_t status = 0u; 
    _beginTransaction();

    uint8_t tx[2] = {ACTIVATE, 0x73}; 
    machine->spi.transfer(_spi, &tx, &status, sizeof(tx));
    
    _endTransaction();

    return NRF24_OK;
}

static nrf24_status_t _readRegister(uint8_t reg, uint8_t *result){
    uint8_t status = 0; 
    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = reg;
    *ptx++ = RF24_NOP; // Dummy operation, just for reading

    machine->spi.transfer(_spi, (const uint8_t*)ptx, prx, 2);

    status = *prx;   // status is 1st byte of receive buffer
    *result = *++prx; // result is 2nd byte of receive buffer

    (void)_endTransaction();

    return NRF24_OK;
};


static nrf24_status_t _readRegisternb(uint8_t reg, uint8_t *buffer, size_t length) {
    uint8_t status = 0; 
    _beginTransaction();
    
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1);

    *ptx++ = reg;
    while (length--) {
        *ptx++ = RF24_NOP; // Dummy operation, just for reading
    }

    machine->spi.transfer(_spi, (const uint8_t *)ptx, prx, size);

    status = *prx++;
 
    while (--size) {
        *buffer++ = *prx++;
    }

    _endTransaction();

    return NRF24_OK;
};


static nrf24_status_t _writeRegister(uint8_t reg, const uint8_t value){
    uint8_t status = 0; 

    (void)printf("write_register(%02x,%02x)\n", reg, value);

    _beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = (W_REGISTER | reg);
    *ptx = value;

    int a = machine->spi.transfer(_spi, (const uint8_t *)ptx, prx, 2);
    printf("SPI transfer returned: %d\n", a);
    
    _endTransaction();
    return NRF24_OK;
};


static nrf24_status_t _writeRegisternb(uint8_t reg, const uint8_t* buffer, uint8_t length){
    uint8_t status = 0; 

    _beginTransaction();
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1); // Add register value to transmit buffer

    *ptx++ = (W_REGISTER | reg);
    while (length--) {
        *ptx++ = *buffer++;
    }

    machine->spi.transfer(_spi, (const uint8_t *)ptx, prx, size);

    status = *prx; // status is 1st byte of receive buffer
    _endTransaction();

    return NRF24_OK;
};


static nrf24_status_t _beginTransaction() {
    machine->spi.beginTransaction(_spi);
    csn(LOW);
    return NRF24_OK;
}

static nrf24_status_t _endTransaction() {
    csn(HIGH);
    machine->spi.endTransaction(_spi);
    return NRF24_OK;
}

static void csn(bool level){
    if (_cfg != NULL){
        machine->gpio.write(_cfg->gpio.csn, level);
        // machine->sleep.ms(1);
    }
}

static void ce(bool level){
    if (_cfg != NULL){
        machine->gpio.write(_cfg->gpio.ce, level);
    }
}