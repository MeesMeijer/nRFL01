#include "inc/hal/config.h"
#include "inc/hal/machine.h"


#ifdef USE_LINUX_LUCKFOX

#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "time.h"
#include "sys/ioctl.h" 
#include "linux/spi/spidev.h" 


/* SPI */
static const int spi_mode[] = {
    SPI_MODE_0, SPI_MODE_1, SPI_MODE_2, SPI_MODE_3 
};
static char device[] = "/dev/spidev0.0";
struct spi_handle {
    int spi_file; 
    bool opened; 
};

/* GPIO */
#define MAX_PATH_LENGTH (50u)
static char o_gpio_path[MAX_PATH_LENGTH]; /* To set the direction of gpio via export */
static char w_gpio_path[MAX_PATH_LENGTH]; /* To set the gpio pin to 0 or 1 */
static char r_gpio_path[MAX_PATH_LENGTH]; /* To get the value of the gpio pin */

/* Sleep/millis */
static long start; /* To keep track of the millis since start. */


/* Begin: machine->spi */
static spi_handle_t* spi_open(uint8_t bus, uint32_t freq_hz, uint8_t mode) {
    spi_handle_t *h = malloc(sizeof(spi_handle_t));

    /* means that bus 11 -> spidev1.1 */
    device[11] += ( (bus/10) % 10);
    device[13] += ( (bus) % 10);

    if ((h->spi_file = open(device, O_RDWR)) < 0 ){
        printf("Failed to open.. \r\n");
    }

    uint8_t bits = 8;
    if (ioctl(h->spi_file, SPI_IOC_WR_MODE, &spi_mode[mode]) < 0) {
        perror("Failed to set SPI mode");
        close(h->spi_file);
        return NULL;
    }
    if (ioctl(h->spi_file, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("Failed to set SPI bits per word");
        close(h->spi_file);
        return NULL;
    }

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
    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)data,
        .rx_buf = (unsigned long)NULL,
        .len = len,
        .delay_usecs = 0,
        .speed_hz = 10*1000*1000,  // SPI speed in Hz
        .bits_per_word = 8
    };

    if (ioctl(h->spi_file, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Failed to perform SPI transfer");
        close(h->spi_file);
        return -1;
    }

    return 1;
}

static int spi_receive(spi_handle_t *h, uint8_t *data, size_t len) {
    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)NULL,
        .rx_buf = (unsigned long)data,
        .len = len,
        .delay_usecs = 0,
        .speed_hz = 10*1000*1000,  // SPI speed in Hz
        .bits_per_word = 8,
    };

    if (ioctl(h->spi_file, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Failed to perform SPI transfer");
        close(h->spi_file);
        return -1;
    }
    
    return 1;
}

static int spi_transmit_receive(spi_handle_t *h, const uint8_t *tx, uint8_t *rx, size_t len) {
    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .delay_usecs = 10u,
        .speed_hz = 10*1000*1000,  // SPI speed in Hz
        .bits_per_word = 8,
        .cs_change = false
    };

    if (ioctl(h->spi_file, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Failed to perform SPI transfer");
        close(h->spi_file);
        return -1;
    }
    return 1;
}

static void spi_close(spi_handle_t *h) {
    if (h) {
        close(h->spi_file);
        free(h);
    }
}

// ================= GPIO ==================
static int gpio_config(uint8_t pin, bool output) {
    FILE *gpio_export = fopen("/sys/class/gpio/export", "w");
    if (gpio_export == NULL){
        printf("Could not open export");
        return -1;
    }

    fprintf(gpio_export, "%d", pin);
    fclose(gpio_export);

    snprintf(o_gpio_path, sizeof(o_gpio_path), "/sys/class/gpio/gpio%d/direction", pin);
    FILE *gpio_direction = fopen(o_gpio_path, "w");
    if (gpio_direction == NULL) {
        perror("Failed to open GPIO direction file");
        return -1;
    }

    if (output == true){
        fprintf(gpio_direction, "out");
    }
    else {
        fprintf(gpio_direction, "in");
    }
    fclose(gpio_direction);
    
    return 0;
}

static int gpio_write(uint8_t pin, bool level) {
    snprintf(w_gpio_path, sizeof(w_gpio_path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *value_file = fopen(w_gpio_path, "w");
    if (value_file == NULL) {
        perror("Failed to open GPIO value file");
        return -1;
    }   

    if (level == LOW){
        fprintf(value_file, "0");
    }
    else{
        fprintf(value_file, "1");
    }

    fclose(value_file);
    return 1;
}

static bool gpio_read(uint8_t pin) {
    snprintf(r_gpio_path, sizeof(r_gpio_path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *value_file = fopen(r_gpio_path, "r");
    if (value_file == NULL) {
        perror("Failed to open GPIO value file for reading");
        return false;
    }
    char value;
    if (fread(&value, 1, 1, value_file) != 1) {
        fclose(value_file);
        return false;
    }
    fclose(value_file);
    return (value == '1');
}

/* Begin: Machine->sleep  */
static void sleep_setup(void){
}
static void sleep_ms(uint32_t ms) {
    usleep(ms*1000);
}
static void sleep_us(uint32_t us){
    usleep(us);
}
/* End: Machine->sleep  */

/* Begin: Machine->time  */
static uint32_t millis(void){
    struct timespec ts; 
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
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