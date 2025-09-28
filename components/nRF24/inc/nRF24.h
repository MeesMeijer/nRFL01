#ifndef NRF24_H
#define NRF24_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

#include "inc/hal/machine.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _BV
    #define _BV(x) (1 << (x))
#endif

#define rf24_max(a, b) ((a) > (b) ? (a) : (b))
#define rf24_min(a, b) ((a) < (b) ? (a) : (b))

#define NRF24_MAX_PAYLOAD_SIZE 32u 
#define NRF24_MAX_RF_CHANNEL   126u

#define NRF25_MAX_ADDRESS_WIDTH 5u 
#define NRF25_MIN_ADDRESS_WIDTH 3u 


typedef enum {
    NRF24_OK, 
    NRF24_ERROR, 
    NRF24_NULL_POINTER,
    NRF24_ARG_INVALID
} nrf24_status_t; 


typedef enum {
    nRF24L01_PLUS, 
    nRF24L01
} nrf24_hw_types_t;

/**
 * @defgroup PALevel Power Amplifier level
 * Power Amplifier level. The units dBm (decibel-milliwatts or dB<sub>mW</sub>)
 * represents a logarithmic signal loss.
 * @{
 */
typedef enum
{
    /**
     * (0) represents:
     * nRF24L01 | Si24R1 with lnaEnabled = 1 | Si24R1 with lnaEnabled = 0
     * :-------:|:-----------------------------:|:----------------------------:
     *  -18 dBm | -6 dBm | -12 dBm
     */
    NRF24_PA_MIN = 0,
    /**
     * (1) represents:
     * nRF24L01 | Si24R1 with lnaEnabled = 1 | Si24R1 with lnaEnabled = 0
     * :-------:|:-----------------------------:|:----------------------------:
     *  -12 dBm | 0 dBm | -4 dBm
     */
    NRF24_PA_LOW,
    /**
     * (2) represents:
     * nRF24L01 | Si24R1 with lnaEnabled = 1 | Si24R1 with lnaEnabled = 0
     * :-------:|:-----------------------------:|:----------------------------:
     *  -6 dBm | 3 dBm | 1 dBm
     */
    NRF24_PA_HIGH,
    /**
     * (3) represents:
     * nRF24L01 | Si24R1 with lnaEnabled = 1 | Si24R1 with lnaEnabled = 0
     * :-------:|:-----------------------------:|:----------------------------:
     *  0 dBm | 7 dBm | 4 dBm
     */
    NRF24_PA_MAX,
    /**
     * (4) This should not be used and remains for backward compatibility.
     */
    NRF24_PA_ERROR
} nrf24_pa_dbm_t;

/**
 * @}
 * @defgroup Datarate datarate
 * How fast data moves through the air. Units are in bits per second (bps).
 * @see
 * @{
 */
typedef enum
{
    /** (0) represents 1 Mbps */
    NRF24_1MBPS = 0,
    /** (1) represents 2 Mbps */
    NRF24_2MBPS,
    /** (2) represents 250 kbps */
    NRF24_250KBPS,
    /*  (3) Have a max rate item for debugging. */
    NRF24_MAX_RATE
} nrf24_datarate_t;

/**
 * @}
 * @defgroup CRCLength CRC length
 * The length of a CRC checksum that is used (if any). Cyclical Redundancy
 * Checking (CRC) is commonly used to ensure data integrity.
 * @see
 * @{
 */
typedef enum
{
    /** (0) represents no CRC checksum is used */
    NRF24_CRC_DISABLED = 0,
    /** (1) represents CRC 8 bit checksum is used */
    NRF24_CRC_8,
    /** (2) represents CRC 16 bit checksum is used */
    NRF24_CRC_16,
    NRF24_CRC_ERROR,
} nrf24_crclength_t;

/**
 * @}
 * @defgroup fifoState FIFO state
 * The state of a single FIFO (RX or TX).
 * Remember, each FIFO has a maximum occupancy of 3 payloads.
 * @see RF24::isFifo()
 * @{
 */
typedef enum
{
    /// @brief The FIFO is not full nor empty, but it is occupied with 1 or 2 payloads.
    NRF24_FIFO_OCCUPIED,
    /// @brief The FIFO is empty.
    NRF24_FIFO_EMPTY,
    /// @brief The FIFO is full.
    NRF24_FIFO_FULL,
    /// @brief Represents corruption of data over SPI (when observed).
    NRF24_FIFO_INVALID,
} nrf24_fifo_state_t;

typedef enum
{
    #include "nRF24L01.h"
    /// An alias of `0` to describe no IRQ events enabled.
    NRF24_IRQ_NONE = 0,
    /// Represents an event where TX Data Failed to send.
    NRF24_TX_DF = 1 << MASK_MAX_RT,
    /// Represents an event where TX Data Sent successfully.
    NRF24_TX_DS = 1 << TX_DS,
    /// Represents an event where RX Data is Ready to `RF24::read()`.
    NRF24_RX_DR = 1 << RX_DR,
    /// Equivalent to `RF24_RX_DR | RF24_TX_DS | RF24_TX_DF`.
    NRF24_IRQ_ALL = (1 << MASK_MAX_RT) | (1 << TX_DS) | (1 << RX_DR),
} nrf24_irq_flags_t;


typedef struct {
    uint8_t addressWidth;
    uint8_t payloadSize; 
    uint8_t channel;
    nrf24_datarate_t datarate;
    nrf24_crclength_t crc; 

    bool    dynamicPayloads;

    struct {
        uint8_t delay;
        uint8_t count;
    } retries; 
    struct {
        bool dynamicAck; 
        bool ackPayload; 
        bool autoAck; 
    } ack; 
    struct {
        uint8_t level;
        bool lnaEnabled; 
    } PA;
    struct {   
        // @brief Chip Select (Inverted)
        int csn;
        // @brief Chip Enable 
        int ce;
    } gpio;
} nrf24_cfg_t;

#define NRF24_DEFAULT_CFG(_csn, _ce) { \
    .addressWidth = NRF25_MAX_ADDRESS_WIDTH, \
    .payloadSize = NRF24_MAX_PAYLOAD_SIZE, \
    .channel = 76u, \
    .datarate = NRF24_1MBPS, \
    .dynamicPayloads = false, \
    .retries = { .delay = 5u, .count = 15u }, \
    .ack = { .ackPayload = false, .autoAck = true }, \
    .crc = NRF24_CRC_16, \
    .PA = { .level = NRF24_PA_MIN, .lnaEnabled = true }, \
    .gpio = { .csn = _csn, .ce = _ce } \
}

extern nrf24_status_t nRF24_init(spi_handle_t *spi, const nrf24_cfg_t *cfg);
extern nrf24_status_t nRF24_isConnected(bool *connected);
extern bool           nRF24_available(void);

extern nrf24_status_t nRF24_powerDown(void);
extern nrf24_status_t nRF24_powerUp(void);

extern nrf24_status_t nRF24_setChannel(uint8_t channel);
extern nrf24_status_t nRF24_setAddressWidth(uint8_t size);
extern nrf24_status_t nRF24_setPayloadSize(uint8_t size);
extern nrf24_status_t nRF24_setRetries(uint8_t delay, uint8_t count);
extern nrf24_status_t nRF24_setPALevel(nrf24_pa_dbm_t level, bool enableLna);
extern nrf24_status_t nRF24_setCrcLength(nrf24_crclength_t length);
extern nrf24_status_t nRF24_setAutoAck(bool enable);
extern nrf24_status_t nRF24_setAckPayload(bool enable);
extern nrf24_status_t nRF24_setDynamicPayloadLength(bool enable);
extern nrf24_status_t nRF24_setDynamicAck(bool enable);

extern nrf24_status_t nRF24_openReadingPipe(uint8_t pipe, const uint8_t *address);
extern nrf24_status_t nRF24_closeReadingPipe(uint8_t pipe);

extern nrf24_status_t nRF24_openWritingPipe(const uint8_t *address);
extern nrf24_status_t nRF24_closeWritingPipe(void);

extern nrf24_status_t nRF24_startListening(void);
extern nrf24_status_t nRF24_stopListening(void); 

extern nrf24_status_t nRF24_read(void *buffer, uint8_t length);
extern nrf24_status_t nRF24_write(const void *buffer, uint8_t length, const bool multicast);
extern nrf24_status_t nRF24_fastWrite(const void *buffer, uint8_t length, const bool multicast);

extern nrf24_status_t nRF24_clearStatusFlags();
extern nrf24_status_t nRF24_getStatusFlags();
extern nrf24_status_t nRF24_update();
extern nrf24_status_t nRF24_flushRx();
extern nrf24_status_t nRF24_flushTx();
extern nrf24_status_t nRF24_isValid();
extern nrf24_status_t nRF24_getVariant();

#ifdef __cplusplus
};
#endif
#endif 