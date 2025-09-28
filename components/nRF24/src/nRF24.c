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
static void _dataRateConversion(nrf24_datarate_t rate, uint8_t *bits); 

static spi_handle_t *_spi = NULL; 
static nrf24_cfg_t  *_cfg  = NULL;

static uint8_t *tx_buffer = NULL;
static uint8_t *rx_buffer = NULL;
static bool    _is_p_variant = false; 

/* For storing the value of the NRF_CONFIG register. 
    Used for determination the state of the device. 
 */
static uint8_t config_reg = 0u;               

/* TxDelay (ms)*/
static uint16_t txDelay = 0u; 


nrf24_status_t nRF24_init(spi_handle_t *spi, const nrf24_cfg_t *cfg){
    nrf24_status_t status = NRF24_OK;

    if ((spi == NULL) || (cfg == NULL)) {
        status = NRF24_NULL_POINTER;
    }
    else if (_spi != NULL){
        status = NRF24_ERROR; 
    }
    else {
        /* Init the NRF24 */
        _spi = spi;
        _cfg = cfg; 

        machine->gpio.config(_cfg->gpio.ce, true);
        machine->gpio.config(_cfg->gpio.csn, true);
        
        ce(LOW);
        machine->sleep.ms(5);
        csn(HIGH);

        tx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 33u);
        rx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * 33u);
        
        status = _initRadio();
    }
    return status; 
};


nrf24_status_t nRF24_isConnected(bool *connected) {
    uint8_t result = 0u; 

    (void)_readRegister(SETUP_AW, &result);
    /* + 2 because the AddressWidth is stored -2 within NRF24 
        See nRF24_setAddressWidth for more info. */
    *connected = ((result + 2u) == _cfg->addressWidth);

    return NRF24_OK;
};

nrf24_status_t nRF24_powerUp(void){
    // if not powered up then power up and wait for the radio to initialize
    if (!(config_reg & _BV(PWR_UP))) {
        config_reg |= _BV(PWR_UP);
        (void)_writeRegister(NRF_CONFIG, config_reg);

        // For nRF24L01+ to go from power down mode to TX or RX mode it must first pass through stand-by mode.
        // There must be a delay of Tpd2stby (see Table 16.) after the nRF24L01+ leaves power down mode before
        // the CEis set high. - Tpd2stby can be up to 5ms per the 1.0 datasheet
        machine->sleep.ms(5u);
    }
    return NRF24_OK;
}

nrf24_status_t nRF24_powerDown(void){
    (void)ce(LOW); // Guarantee CE is low on powerDown
    config_reg = (uint8_t)(config_reg & ~_BV(PWR_UP));
    (void)_writeRegister(NRF_CONFIG, config_reg);
    return NRF24_OK;
}


nrf24_status_t nRF24_setPayloadSize(uint8_t size){
    nrf24_status_t status = NRF24_OK; 

    uint8_t idx = 0u;
    uint8_t pipe_req = 0u;

    if (size > 32u){
        status = NRF24_ARG_INVALID;
    } else {
        /* Sync with global _cfg struct */
        _cfg->payloadSize = size; 

        /* Ensure that all pipes have the same payload size */
        for (uint8_t i = 0u; i < 6u; ++i) {
            pipe_req = RX_PW_P0 + i; 
            status = _writeRegister(pipe_req, size);
        }
    }

    return NRF24_OK;
};


nrf24_status_t nRF24_setAddressWidth(uint8_t size){
    nrf24_status_t status = NRF24_OK;

    if ((size < 3u) || (size > 5u)) {
        status = NRF24_ARG_INVALID;
    } else {
        /* Set the address width in the global config */
        _cfg->addressWidth = size;

        /* SETUP_AW register expects: 0x01 for 3 bytes, 0x02 for 4 bytes, 0x03 for 5 bytes */
        uint8_t reg_val = size - 2u; // Ensure only LSB is used
        status = _writeRegister(SETUP_AW, reg_val);
    }
    return NRF24_OK;
}

nrf24_status_t nRF24_setRetries(uint8_t delay, uint8_t count){
    nrf24_status_t status = NRF24_OK;

    if ( delay > 15u ) {
        status = NRF24_ARG_INVALID;     
    } else if ( count > 15u ){
        status = NRF24_ARG_INVALID;
    } else {
        /* SETUP_RETR: 0000  |  0000 
                       Delay | Count
            So make sure delay is left shifted 4 bits. 
        */
        status = _writeRegister(SETUP_RETR, (uint8_t) ((delay << 4u)|( count )) );
    }

    return status; 
}

nrf24_status_t nRF24_setDataRate(nrf24_datarate_t rate){
    nrf24_status_t status         = NRF24_OK; 
    uint8_t        current_rate   = 0u; 
    uint8_t        new_rate       = 0u; 

    /* 
        RF_SETUP: |     7      |  6   |     5     |    4     |     3      |   2:1  |     0     | 
                  | CONST_WAVE | Res  | RF_DR_LOW | PLL_LOCK | RF_DR_HIGH | RF_PWR | Obsolete  |
        
        CONST_WAVE: Enables continuous carrier transmit when high.
        Res(verd) : Dont use. 
        RF_DR_LOW : Set RF Data Rate to 250kbps.
        PLL_LOCK  : Dont Use.. Only for testing. 
        RF_DR_HIGH: Encoded as: [RF_DR_LOW, RF_DR_HIGH]
            00 : 1Mbps.
            01 : 2Mbps. 
            10 : 250kbps
            11 : Reserved. 
        RF_PWR: Set Output power in TX mode. 
            00 : -18 dBm
            01 : -12 dBm 
            10 :  -6 dBm 
            11 :   0 dBm 
        Obsolete: Don't use. 
    */

    if (rate >= NRF24_MAX_RATE){
        status = NRF24_ARG_INVALID;
    } else if (_readRegister(RF_SETUP, &current_rate) != NRF24_OK) {
        status = NRF24_ERROR;
    } else {
        (void)_dataRateConversion(rate, &new_rate);

        current_rate = (current_rate & ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH)));
        current_rate |= new_rate; 

        status = _writeRegister(RF_SETUP, current_rate);

        /* Check if the register is set correctly, reuse new_rate */
        status = _readRegister(RF_SETUP, &new_rate);
        if (new_rate == current_rate){
            status = NRF24_OK;    
        }
        else {
            status = NRF24_ERROR;
        }
    }
    return status; 
}

nrf24_status_t nRF24_setChannel(uint8_t channel){
    nrf24_status_t status = NRF24_OK;

    if (channel > 125u) {
        status = NRF24_ARG_INVALID; 
    } else {
        status = _writeRegister(RF_CH, channel);
    }

    return status; 
};

nrf24_status_t nRF24_flushRx(){
    (void)printf("[NRF24] - Flushing RX\n");
    return _readRegisternb(FLUSH_RX, NULL, 0);
};

nrf24_status_t nRF24_flushTx(){
    (void)printf("[NRF24] - Flushing TX\n");
    return _readRegisternb(FLUSH_TX, NULL, 0);
};


/* Static Functions */

static nrf24_status_t _initRadio() {
    uint8_t status     = 0u; 

    /* Sleep 5 ms to allow the radio to settle. */
    machine->sleep.ms(5u);

    /* Set retries: Delay 5 ms, Retries: 15 */
    (void)nRF24_setRetries(5u, 15u);

    /* Set Datarate: 1MBPS */
    (void)nRF24_setDataRate(NRF24_1MBPS);    

    uint8_t before_toggle; 
    (void)_readRegister(FEATURE, &before_toggle);

    /* Toggle features */
    (void)_toggleFeatures();

    uint8_t after_toggle;
    (void)_readRegister(FEATURE, &after_toggle);

    printf("[nRF24] _initRadio: FEATURE before toggle: 0x%02X\n", before_toggle);
    printf("[nRF24] _initRadio: FEATURE after_toggle: 0x%02X\n", after_toggle);

    _is_p_variant = before_toggle == after_toggle;
    if (after_toggle) {
        if (_is_p_variant) {
            // module did not experience power-on-reset (#401)
            (void)_toggleFeatures();
        }
        // allow use of multicast parameter and dynamic payloads by default
        (void)_writeRegister(FEATURE, 0u);
    }

    /* Disable dynamic payloads by default (for all pipes) */
    (void)_writeRegister(DYNPD, 0u);     

    /* Enable Auto-Ack on all Pipes */
    (void)_writeRegister(EN_AA, (const uint8_t)0x3F);  
    /* Only open RX pipe 0 & 1 */
    (void)_writeRegister(EN_RXADDR, 3u);

    (void)nRF24_setPayloadSize(32u);
    (void)nRF24_setAddressWidth(5u);

    (void)nRF24_setChannel(76u);

    /*  Reset current status. 
        Notice reset 
    */
    (void)_writeRegister(NRF_STATUS, NRF24_IRQ_ALL);

    /* And Flush rx/tx fifo buffers*/
    (void)nRF24_flushRx();
    (void)nRF24_flushTx();

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
    
    (void)_beginTransaction();

    uint8_t tx[2] = {ACTIVATE, 0x73}; 
    uint8_t status[2];

    machine->spi.transfer(_spi, (const uint8_t *)&tx, (uint8_t *)&status, sizeof(tx));
    
    (void)_endTransaction();

    return NRF24_OK;
}


static nrf24_status_t _readRegister(uint8_t reg, uint8_t *result){
    uint8_t status = 0; 

    (void)printf("[NRF24] - read_register(%02x) -> ", reg);
    
    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = reg;
    *ptx++ = RF24_NOP; // Dummy operation, just for reading

    machine->spi.transfer(_spi, (const uint8_t*)tx_buffer, rx_buffer, 2);

    status = *prx;   // status is 1st byte of receive buffer
    *result = *++prx; // result is 2nd byte of receive buffer

    (void)_endTransaction();

    (void)printf("%02x (%c%c%c%c %c%c%c%c) status: %02X\n", *result,
            (*result & 0x80) ? '1' : '0',
            (*result & 0x40) ? '1' : '0',
            (*result & 0x20) ? '1' : '0',
            (*result & 0x10) ? '1' : '0',
            (*result & 0x08) ? '1' : '0',
            (*result & 0x04) ? '1' : '0',
            (*result & 0x02) ? '1' : '0',
            (*result & 0x01) ? '1' : '0',
            status
        );

    return NRF24_OK;
};


static nrf24_status_t _readRegisternb(uint8_t reg, uint8_t *buffer, size_t length) {
    uint8_t status = 0; 
    
    printf("[NRF24] - _readRegisternb(%02X, buff, %d)\n", reg, length);

    (void)_beginTransaction();
    
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1u);

    *ptx++ = reg;
    while (length--) {
        *ptx++ = RF24_NOP; // Dummy operation, just for reading
    }

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, size);

    status = *prx++;
 
    while (--size) {
        *buffer++ = *prx++;
    }

    (void)_endTransaction();

    return NRF24_OK;
};


static nrf24_status_t _writeRegister(uint8_t reg, const uint8_t value){
    uint8_t status = 0; 

    (void)printf("[NRF24] - write_register(%02X,%02X)\n", reg, value);

    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = (W_REGISTER | reg);
    *ptx = value;

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, 2);
    
    (void)_endTransaction();
    return NRF24_OK;
};


static nrf24_status_t _writeRegisternb(uint8_t reg, const uint8_t* buffer, uint8_t length){
    uint8_t status = 0; 

    (void)printf("[NRF24] - _writeRegisternb(%02X, buff, %d)\n", reg, length);

    (void)_beginTransaction();
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1u); // Add register value to transmit buffer

    *ptx++ = (W_REGISTER | reg);
    while (length--) {
        *ptx++ = *buffer++;
    }

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, size);

    status = *prx; // status is 1st byte of receive buffer
    (void)_endTransaction();

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

static void _dataRateConversion(nrf24_datarate_t rate, uint8_t *bits){
    if (rate == NRF24_250KBPS) {
#if !defined(F_CPU) || F_CPU > 20000000
        txDelay = 505u; 
#else /* 16MHZ Arduino */
        txDelay = 155u; 
#endif 
        *bits = (uint8_t)_BV(RF_DR_LOW);
    } 
    
    else if (rate == NRF24_1MBPS) {
#if !defined(F_CPU) || F_CPU > 20000000
        txDelay = 280u;
#else  /* 16MHZ Arduino */
        txDelay = 85u; 
#endif 
        *bits = (uint8_t)0u;
    } 
    
    else if (rate == NRF24_2MBPS) {
#if !defined(F_CPU) || F_CPU > 20000000
        txDelay = 240u; 
#else /* 16MHZ Arduino */
        txDelay = 65u; 
#endif 
        *bits = (uint8_t)_BV(RF_DR_HIGH);
    }

}; 

static void csn(bool level){
    if (_cfg != NULL){
        machine->gpio.write(_cfg->gpio.csn, level);
    }
}

static void ce(bool level){
    if (_cfg != NULL){
        machine->gpio.write(_cfg->gpio.ce, level);
    }
}