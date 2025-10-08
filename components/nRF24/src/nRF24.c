#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "inc/hal/config.h"
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
static bool _isinTxMode(void);

static nrf24_status_t _writePayload(const void *buffer, uint8_t length, const bool multicast);
static nrf24_status_t _readPayload(void *buffer, uint8_t length);
static uint8_t _updateStatus(void);

static spi_handle_t *_spi = NULL; 
static nrf24_cfg_t  *_cfg  = NULL;

static uint8_t *tx_buffer = NULL;
static uint8_t *rx_buffer = NULL;
static bool    _is_p_variant = false; 

/* For storing the value of the NRF_CONFIG register. 
    Used for determination the state of the device. 
 */
static uint8_t config_reg = 0u;

static uint8_t nrf24_spi_status = 0u; 

/* TxDelay (ms)*/
static uint16_t txDelay = 0u;
static bool pipe0_is_rx = false; 

typedef struct {
    uint8_t  num;
    uint8_t *writeAddress;
    uint8_t *readAddress; 
} nrf24_pipe_t; 

static nrf24_pipe_t pipe0_cfg = {.num = 0};

static const uint8_t pipe_reg_address[] = {
    RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2,
    RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};

static const uint8_t pipe_enn_bits[] = {
    ERX_P0, ERX_P1, ERX_P2,
    ERX_P3, ERX_P4, ERX_P5
};

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
        _cfg = (nrf24_cfg_t *)cfg; 
        
        machine->gpio.config(_cfg->gpio.ce, true);
        machine->gpio.config(_cfg->gpio.csn, true);
        
        (void)ce(LOW);
        /* Let the CE Settle */
        machine->sleep.ms(5);
        
        (void)csn(HIGH);

        tx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * NRF24_MAX_PAYLOAD_SIZE + 1u);
        rx_buffer = (uint8_t *)malloc(sizeof(uint8_t) * NRF24_MAX_PAYLOAD_SIZE + 1u);
        
        pipe0_cfg.readAddress =  (uint8_t *)malloc(sizeof(uint8_t) * 5u);
        pipe0_cfg.writeAddress = (uint8_t *)malloc(sizeof(uint8_t) * 5u);

        status = _initRadio();
    }
    return status; 
};


nrf24_status_t nRF24_isConnected(bool *connected) {
    nrf24_status_t status = NRF24_OK;
    uint8_t        result = 0u; 

    status = _readRegister(SETUP_AW, &result);
    /* + 2 because the AddressWidth is stored -2 within NRF24 
        See nRF24_setAddressWidth for more info. */
    *connected = ((result + 2u) == _cfg->addressWidth);

    return status;
};

bool nRF24_available(void) {
    uint8_t result = 0u; 
    (void)_readRegister(FIFO_STATUS, &result);

    return ( (result & 1) == 0);
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

    if (size > NRF24_MAX_PAYLOAD_SIZE){
        status = NRF24_ARG_INVALID;
    } else {
        /* Ensure that all pipes have the same payload size */
        for (idx = 0u; idx < 6u; ++idx) {
            pipe_req = RX_PW_P0 + idx; 
            status = _writeRegister(pipe_req, size);
        }
    }

    if (status == NRF24_OK){
        /* Sync with global _cfg struct */
        _cfg->payloadSize = size; 
    }

    return status;
};


nrf24_status_t nRF24_setAddressWidth(uint8_t size){
    nrf24_status_t status = NRF24_OK;

    if ((size < NRF24_MIN_ADDRESS_WIDTH) || (size > NRF24_MAX_ADDRESS_WIDTH)) {
        status = NRF24_ARG_INVALID;
    } else {
        /* SETUP_AW register expects: 0x01 for 3 bytes, 0x02 for 4 bytes, 0x03 for 5 bytes */
        uint8_t reg_val = size - 2u;
        status = _writeRegister(SETUP_AW, reg_val);
    }

    if (status == NRF24_OK){
        /* Set the address width in the global config */
        _cfg->addressWidth = size;
    }

    return status;
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
            1111 == 15, so check max value for 15.  
        */
        status = _writeRegister(SETUP_RETR, ((delay << 4u) | ( count )) );
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->retries.count = count;
        _cfg->retries.delay = delay;
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
        Obsolete: Don't care. 
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

        /* Check if the register is set correctly, !! reused new_rate !! */
        status = _readRegister(RF_SETUP, &new_rate);
        if (new_rate == current_rate){
            status = NRF24_OK;
        }
        else {
            status = NRF24_ERROR;
        }
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->datarate = rate; 
    }

    return status; 
}

nrf24_status_t nRF24_setChannel(uint8_t channel){
    nrf24_status_t status = NRF24_OK;

    if (channel > NRF24_MAX_RF_CHANNEL) {
        status = NRF24_ARG_INVALID; 
    } else {
        status = _writeRegister(RF_CH, channel);
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->channel = channel; 
    }

    return status; 
};

nrf24_status_t nRF24_setPALevel(nrf24_pa_dbm_t level, bool enableLna){
    nrf24_status_t status = NRF24_OK;
    uint8_t current_setup = 0u; 

    if (level >= NRF24_PA_ERROR){
        status = NRF24_ARG_INVALID;
    } else if (_readRegister(RF_SETUP, &current_setup) != NRF24_OK) {
        status = NRF24_ERROR;
    } else {
        /* Clear the interested bits.  */
        current_setup &= 0xF8; /* Same as: 1111 1000 */

        /* First 3 bits allow for setting the PA levels. 
             To support v1, also set bit: 0 for enableLna
        */
        current_setup |= (level << 1u) + enableLna;

        status = _writeRegister(RF_SETUP, current_setup);
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->PA.level = level; 
        _cfg->PA.lnaEnabled = enableLna; 
    }

    return status; 
};

nrf24_status_t nRF24_setCrcLength(nrf24_crclength_t length){
    nrf24_status_t status = NRF24_OK;
    // uint8_t current_config = 0u; 

    if (length >= NRF24_CRC_ERROR){
        status = NRF24_ARG_INVALID;
    } else if (_readRegister(NRF_CONFIG, &config_reg) != NRF24_OK){
        status = NRF24_ERROR;
    } else {
        /* Clear the current bits: 3 (CRC_EN) and 2 (Crc_length) */
        config_reg &= ~(_BV(CRCO) | _BV(EN_CRC) );

        if (length == NRF24_CRC_DISABLED){
            /* Clear bit 2, done in previous step */
        } else if (length == NRF24_CRC_8){
            /* Clear bit 2, set bit 3*/
            config_reg |= _BV(EN_CRC);
        } else if (length == NRF24_CRC_16){
            /* Set both bits 3 and 2. */
            config_reg |= (_BV(CRCO) | _BV(EN_CRC));
        }

        status = _writeRegister(NRF_CONFIG, config_reg);
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->crc = length;
    }

    return status; 
}
/**
 * @brief Dis/enables the Auto Acknowledgment feature for all pipes.
 * 
 * ! Also disables ackPayloads if enabled. Because ackpayloads need autoAck to work. 
 * 
 * @param enable 
 * @return nrf24_status_t 
 */
nrf24_status_t nRF24_setAutoAck(bool enable){
    nrf24_status_t status = NRF24_OK; 

       
    if (enable == true){
        /* Write: 0011 1111 
            Auto ack for all pipes. 
        */
        status = _writeRegister(EN_AA, 0x3F); 
    } else {
        if (_writeRegister(EN_AA, 0) != NRF24_OK){
            status = NRF24_ERROR;
        } else if (_cfg->ack.ackPayload){
            /* Also disableAckPayload.. */
            status = nRF24_setAckPayload(false);
        }
    }

    if (status == NRF24_OK){
        /* Update _cfg */
        _cfg->ack.autoAck = enable;
    }

    return status; 
}

/**
 * @brief Dis/enable payloads for acknowlments. 
 * 
 * ! Also enables dynamic payloads if not enabled.
 * 
 * @param enable 
 * @return * nrf24_status_t 
 */
nrf24_status_t nRF24_setAckPayload(bool enable){
    nrf24_status_t status          = NRF24_OK; 
    uint8_t        current_feature = 0u;

    if (_readRegister(FEATURE, &current_feature) != NRF24_OK){
        status = NRF24_ERROR;
    } else if (enable == true){
        /* Enable AckPayloads, and DynamicPayloads. */
        current_feature |= (_BV(EN_ACK_PAY));
        if (_writeRegister(FEATURE, current_feature) != NRF24_OK) {
            status = NRF24_ERROR;
        } else if (nRF24_setDynamicPayloadLength(true) != NRF24_OK){
            status = NRF24_ERROR;
        }
    } else if (enable == false) {
        /* Disable AckPayloads, but ignore dynamic payloads.. */
        current_feature &= ~_BV(EN_ACK_PAY);
        status = _writeRegister(FEATURE, current_feature);
    }

    if (status == NRF24_OK){
        _cfg->ack.ackPayload = enable; 
    }
    
    return status; 
}

/**
 * @brief Dis/enable dynamicPayloads. 
 * 
 * Requires: Auto Ack on all pipes.. ENAA_P1/5
 * 
 * @param enable 
 * @return * nrf24_status_t 
 */
nrf24_status_t nRF24_setDynamicPayloadLength(bool enable){
    nrf24_status_t status        = NRF24_OK; 
    uint8_t        current_dynpd   = 0u;
    uint8_t        current_feature = 0u;

    if (_readRegister(DYNPD, &current_dynpd) != NRF24_OK){
        status = NRF24_ERROR;
    } 
    else if (_readRegister(FEATURE, &current_feature) != NRF24_OK){
        status = NRF24_ERROR;
    } 
    else if (enable == true){
        /* Enable dynamic length on all pipes */
        current_dynpd |= (_BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));
        status = _writeRegister(DYNPD, current_dynpd);

        /* Enable DPL feature */
        current_feature |= _BV(EN_DPL);
        status = _writeRegister(FEATURE, current_feature);

        /* Enable Auto Ack */
        status = nRF24_setAutoAck(true);
    } 
    else if (enable == false){
        /* Disable dynamic length on all pipes */
        current_dynpd = 0u; 
        status = _writeRegister(DYNPD, current_dynpd);

        /* Disable DPL feature */
        current_feature &= ~_BV(EN_DPL);
        status = _writeRegister(FEATURE, current_feature);

        /* Dont disable autoack.. let the user decided.. */
    }

    if (status == NRF24_OK){
        _cfg->dynamicPayloads = enable; 
    }

    return status; 
}; 

/**
 * @brief 
 * 
 * @param enable 
 * @return * nrf24_status_t 
 */
nrf24_status_t nRF24_setDynamicAck(bool enable){
    nrf24_status_t status          = NRF24_OK;
    uint8_t        current_feature = 0u;
    
    if (_readRegister(FEATURE, &current_feature) != NRF24_OK){
        status = NRF24_ERROR;
    } else if (enable == true){
        current_feature |= _BV(EN_DYN_ACK);
        status = _writeRegister(FEATURE, current_feature);
    } else if (enable == false){
        current_feature &= ~_BV(EN_DYN_ACK);
        status = _writeRegister(FEATURE, current_feature);        
    }

    if (status == NRF24_OK){
        _cfg->ack.dynamicAck = enable;
    }

    return status; 
}

nrf24_status_t nRF24_flushRx(){
    (void)printf("[NRF24] - Flushing RX\r\n");
    return _readRegisternb(FLUSH_RX, NULL, 0);
};

nrf24_status_t nRF24_flushTx(){
    (void)printf("[NRF24] - Flushing TX\r\n");
    return _readRegisternb(FLUSH_TX, NULL, 0);
};


nrf24_status_t nRF24_openReadingPipe(uint8_t pipe, const uint8_t *address){
    nrf24_status_t status = NRF24_OK; 
    uint8_t current_rxaddre = 0u;

    if (pipe > 5u){
        status = NRF24_ARG_INVALID;
    } else {
        if (pipe == 0u){
            (void)memcpy(pipe0_cfg.readAddress, address, _cfg->addressWidth);
            pipe0_is_rx = true;    
        }

        if (pipe > 1u){
            // For pipes 2-5, only write the LSB
            _writeRegisternb(pipe_reg_address[pipe], address, 1);
        }
        // avoid overwriting the TX address on pipe 0 while still in TX mode.
        // NOTE, the cached RX address on pipe 0 is written when startListening() is called.
        else if (_isinTxMode() || pipe != 0){
            _writeRegisternb(pipe_reg_address[pipe], address, _cfg->addressWidth);
        }
        
        // Note it would be more efficient to set all of the bits for all open
        // pipes at once.  However, I thought it would make the calling code
        // more simple to do it this way.
        (void)_readRegister(EN_RXADDR, &current_rxaddre);
        _writeRegister(EN_RXADDR,  ( current_rxaddre | _BV(pipe_enn_bits[pipe])) );
    }

    return status; 
};

nrf24_status_t nRF24_closeReadingPipe(uint8_t pipe){
    uint8_t current_rxaddr = 0u; 

    _readRegister(EN_RXADDR, &current_rxaddr);
    _writeRegister(EN_RXADDR, (current_rxaddr & ~_BV(pipe_enn_bits[pipe])));

    if (!pipe) {
        // keep track of pipe 0's RX state to avoid null vs 0 in addr cache
        pipe0_is_rx = false;
    }
    return NRF24_OK;
};

nrf24_status_t nRF24_startListening(void){
    
    config_reg |= _BV(PRIM_RX);
    _writeRegister(NRF_CONFIG, config_reg);
    _writeRegister(NRF_STATUS, NRF24_IRQ_ALL);
    ce(HIGH);

    if (pipe0_is_rx){
        _writeRegisternb(RX_ADDR_P0, pipe0_cfg.readAddress, _cfg->addressWidth);
    } else{
        nRF24_closeReadingPipe(0);
    }

    return NRF24_OK;
}

nrf24_status_t nRF24_stopListening(void){
    uint8_t current_addr_p0 = 0u; 

    ce(LOW);

    machine->sleep.ms(1);
    if (_cfg->ack.ackPayload){
        nRF24_flushTx();
    }

    config_reg = (config_reg & ~_BV(PRIM_RX));
    _writeRegister(NRF_CONFIG, config_reg);

    _writeRegisternb(RX_ADDR_P0, pipe0_cfg.writeAddress, _cfg->addressWidth);

    _readRegister(RX_ADDR_P0, &current_addr_p0);
    _writeRegister(EN_RXADDR, (current_addr_p0 | _BV(pipe_enn_bits[0]))); // Enable RX on pipe0
    return NRF24_OK;
}

nrf24_status_t nRF24_openWritingPipe(const uint8_t *address){
    _writeRegisternb(RX_ADDR_P0, address, _cfg->addressWidth);
    _writeRegisternb(TX_ADDR, address, _cfg->addressWidth);
    memcpy(pipe0_cfg.writeAddress, address, _cfg->addressWidth);
    return NRF24_OK;
}

nrf24_status_t nRF24_read(void *buffer, uint8_t length){
    nrf24_status_t status = NRF24_OK;
    
    status = _readPayload(buffer, length);

    /* Clear the IRQ.  */
    status = _writeRegister(NRF_STATUS, NRF24_RX_DR);
    return status;
};

nrf24_status_t nRF24_write(const void *buffer, uint8_t length, const bool multicast){
    nrf24_status_t status = NRF24_OK;

    status = _writePayload(buffer, length, multicast);

    /* Clear the IRQ.  */
    // _writeRegister(NRF_STATUS, NRF24_RX_DR);
    ce(HIGH);
    return status;
};

nrf24_status_t nRF24_fastWrite(const void *buffer, uint8_t length, const bool multicast){
    nrf24_status_t status = NRF24_OK;

    // Needed for linux: uint32_t timer = machine->sleep.millis(); 

    while ( _updateStatus() & _BV(TX_FULL)){
        if (nrf24_spi_status & NRF24_TX_DF){
            return NRF24_ERROR;
        }

        // Needed for linux: if ((machine->sleep.millis() - timer) > 95u){
        //     printf("help\n");
        // }
    }

    status = _writePayload(buffer, length, multicast);
    ce(HIGH);

    return status;
};


/* Static Functions */

static nrf24_status_t _initRadio() {
    // nrf24_status_t status = NRF24_OK; 

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

    printf("[nRF24] _initRadio: FEATURE before toggle: 0x%02X\r\n", before_toggle);
    printf("[nRF24] _initRadio: FEATURE after_toggle: 0x%02X\r\n", after_toggle);

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
    (void)nRF24_setDynamicPayloadLength(false);

    /* Enable Auto-Ack on all Pipes */
    (void)nRF24_setAutoAck(true);

    /* Only open RX pipe 0 & 1 */
    (void)_writeRegister(EN_RXADDR, 3u);

    (void)nRF24_setPayloadSize(NRF24_MAX_PAYLOAD_SIZE);
    (void)nRF24_setAddressWidth(5u);

    (void)nRF24_setChannel(76u);

    /*  Reset current status. 
        Notice reset 
    */
    (void)_writeRegister(NRF_STATUS, NRF24_IRQ_ALL);

    /* And Flush rx/tx fifo buffers*/
    (void)nRF24_flushRx();
    (void)nRF24_flushTx();


    (void)nRF24_setCrcLength(NRF24_CRC_16);
    (void)_readRegister(NRF_CONFIG, &config_reg);

    (void)nRF24_powerUp();
    
    return (config_reg == (_BV(EN_CRC) | _BV(CRCO) | _BV(PWR_UP))) ? NRF24_OK : NRF24_ERROR;
}

static nrf24_status_t _toggleFeatures(void){
    nrf24_status_t status = NRF24_OK;

    (void)_beginTransaction();

    uint8_t tx[2u] = {ACTIVATE, 0x73}; 
    uint8_t rx[2u];

    (void)machine->spi.transfer(_spi, (const uint8_t *)&tx, (uint8_t *)&rx, sizeof(tx));
    
    /* Stop compiler error -unused-variable */
    (void)rx;

    (void)_endTransaction();

    return status;
}


static nrf24_status_t _readRegister(uint8_t reg, uint8_t *result){
    nrf24_status_t status = NRF24_OK;

    #ifdef NRF24_DEBUG
    (void)printf("[NRF24] - _readRegister(%02x) -> ", reg);
    #endif 

    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = reg;
    *ptx++ = RF24_NOP; // Dummy operation, just for reading

    machine->spi.transfer(_spi, (const uint8_t*)tx_buffer, rx_buffer, 2u);

    nrf24_spi_status = *prx;   // status is 1st byte of receive buffer
    *result = *++prx; // result is 2nd byte of receive buffer

    (void)_endTransaction();

    #ifdef NRF24_DEBUG
    (void)printf("%02x (%c%c%c%c %c%c%c%c) status: %02X\r\n", *result,
        (*result & 0x80) ? '1' : '0',
        (*result & 0x40) ? '1' : '0',
        (*result & 0x20) ? '1' : '0',
        (*result & 0x10) ? '1' : '0',
        (*result & 0x08) ? '1' : '0',
        (*result & 0x04) ? '1' : '0',
        (*result & 0x02) ? '1' : '0',
        (*result & 0x01) ? '1' : '0',
        nrf24_spi_status
    );
    #endif 

    return status;
};


static nrf24_status_t _readRegisternb(uint8_t reg, uint8_t *buffer, size_t length) {
    nrf24_status_t status = NRF24_OK;
    
    #ifdef NRF24_DEBUG
    (void)printf("[NRF24] - _readRegisternb(%02X, buff, %d)\r\n", reg, length);
    #endif 

    (void)_beginTransaction();
    
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1u);

    *ptx++ = reg;
    while (length--) {
        *ptx++ = RF24_NOP; // Dummy operation, just for reading
    }

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, size);

    nrf24_spi_status = *prx++;
 
    while (--size) {
        *buffer++ = *prx++;
    }

    (void)_endTransaction();

    return status;
};


static nrf24_status_t _writeRegister(uint8_t reg, const uint8_t value){
    nrf24_status_t status = NRF24_OK; 
    
    #ifdef NRF24_DEBUG
    (void)printf("[NRF24] - _writeRegister(%02X,%02X)\r\n", reg, value);
    #endif

    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    *ptx++ = (W_REGISTER | reg);
    *ptx = value;

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, 2);
    
    nrf24_spi_status = *prx; 

    (void)_endTransaction();
    return status;
};


static nrf24_status_t _writeRegisternb(uint8_t reg, const uint8_t* buffer, uint8_t length){
    nrf24_status_t status = NRF24_OK;

    #ifdef NRF24_DEBUG
    (void)printf("[NRF24] - _writeRegisternb(%02X, buff, %d)\r\n", reg, length);
    #endif 

    (void)_beginTransaction();
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size = (uint8_t)(length + 1u); // Add register value to transmit buffer

    *ptx++ = (W_REGISTER | reg);
    while (length--) {
        *ptx++ = *buffer++;
    }

    machine->spi.transfer(_spi, (const uint8_t *)tx_buffer, rx_buffer, size);

    nrf24_spi_status = *prx; // status is 1st byte of receive buffer
    (void)_endTransaction();

    return status;
};


static nrf24_status_t _writePayload(const void *buffer, uint8_t length, const bool multicast){
    nrf24_status_t status = NRF24_OK; 

    const uint8_t *current = (const uint8_t *)buffer;  
    
    uint8_t blank_len = !length ? 1u : 0u;
    if (length > NRF24_MAX_PAYLOAD_SIZE){
        return NRF24_ARG_INVALID;
    } 

    if (!_cfg->dynamicPayloads) {
        length = rf24_min(length, _cfg->payloadSize);
        blank_len = (_cfg->payloadSize - length);
    }
    else {
        length = rf24_min(length, NRF24_MAX_PAYLOAD_SIZE);
    }

    #ifdef NRF24_DEBUG
    printf("[NRF24] - _writePayload(Writing %u bytes %u blanks)\r\n", length, blank_len);
    #endif 

    _beginTransaction();
    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;
    uint8_t size;
    size = (length + blank_len + 1u); // Add register value to transmit buffer

    if (multicast){
        *ptx++ = W_TX_PAYLOAD_NO_ACK;
    } else{
        *ptx++ = W_TX_PAYLOAD;
    }

    while (length--) {
        *ptx++ = *current++;
    }

    while (blank_len--) {
        *ptx++ = 0u;
    }

    (void)machine->spi.transfer(_spi, tx_buffer, rx_buffer, size);
    
    nrf24_spi_status = *prx++;

    (void)_endTransaction();

    return status; 
};

static nrf24_status_t _readPayload(void *buffer, uint8_t length){
    nrf24_status_t status = NRF24_OK; 
    uint8_t blank_len = 0u;

    uint8_t *current = (uint8_t *)buffer;  

    if (length > NRF24_MAX_PAYLOAD_SIZE){
        return NRF24_ARG_INVALID;
    } 

    if (!_cfg->dynamicPayloads) {
        length = rf24_min(length, _cfg->payloadSize);
        blank_len = (_cfg->payloadSize - length);
    }
    else {
        length = rf24_min(length, NRF24_MAX_PAYLOAD_SIZE);
    }

    (void)_beginTransaction();

    uint8_t* prx = rx_buffer;
    uint8_t* ptx = tx_buffer;

    uint8_t size = (length + blank_len + 1u); // Add register value to transmit buffer

    *ptx++ = R_RX_PAYLOAD;
    while (--size) {
        *ptx++ = RF24_NOP;
    }

    size = (length + blank_len + 1u); // Size has been lost during while, re affect

    machine->spi.transfer(_spi, tx_buffer, rx_buffer, size);

    nrf24_spi_status = *prx++; // 1st byte is status

    if (length > 0u) {
        // Decrement before to skip 1st status byte
        while (--length) {
            *current++ = *prx++;
        }

        *current = *prx;
    }

    (void)_endTransaction();
    
    return status; 
};

static nrf24_status_t _beginTransaction() {
    (void)machine->spi.beginTransaction(_spi);
    (void)csn(LOW);
    return NRF24_OK;
}

static nrf24_status_t _endTransaction() {
    (void)csn(HIGH);
    (void)machine->spi.endTransaction(_spi);
    return NRF24_OK;
}

static uint8_t _updateStatus(void){
    (void)_readRegisternb(RF24_NOP, NULL, 0);
    return nrf24_spi_status;
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
        (void)machine->gpio.write(_cfg->gpio.csn, level);
    }
}

static void ce(bool level){
    if (_cfg != NULL){
        (void)machine->gpio.write(_cfg->gpio.ce, level);
    }
}

static bool _isinTxMode(void){
    return (config_reg & _BV(PRIM_RX));
}