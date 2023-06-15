// Author: Christoph Deussen
//
// Code for sub device of wheel load system.
// Reading from loadscales and sending it over SPI on request.
// Calibration possible via USB V-COM terminal.

#define SET_BIT(x, pos)		(x |= (1U << pos))
#define CLEAR_BIT(x, pos)	(x &= (~(1U << pos)))
#define TOGGLE_BIT(x, pos)	(x ^= (1U << pos))

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/flash.h" // for the flash erasing and writing
#include "hardware/sync.h" // for the interrupts

#include "hx71708.h"

#define FLASH_TARGET_OFFSET (512 * 1024) // choosing to start at 512K

#define BUF_LEN         4

#define LED_PIN         25
#define SPI_COM_PORT    spi0
#define SPI_COM_RX      16
#define SPI_COM_TX      19
#define SPI_COM_SCK     18
#define SPI_COM_CS      17

HX71708_t hx1 = { .dout = HX1_DOUT, .sck = HX1_SCK, .offset = 0, .offset_counter = 0 };
HX71708_t hx2 = { .dout = HX2_DOUT, .sck = HX2_SCK, .offset = 0, .offset_counter = 0 };

volatile int timerval1 = 0;
volatile int timerval2 = 0;

uint32_t time_now   = 0;
uint32_t time_last  = 0;
int32_t hx1_data    = 0;
int32_t hx2_data    = 0;

uint8_t out_buf[4], in_buf[5];

typedef enum Console_Mode { debug_out, calib_in } CONSOLE_MODE_t;
CONSOLE_MODE_t console_mode = debug_out;

char calib_val[4] = { 'x', 'x', 'x', 'x' };
uint8_t calib_counter = 0;
volatile int calib_int = 0;

void save_calib_data();
void read_calib_data();

// Callback for SPI communication. When chip select goes high,
// disconnect push-pull stage from and set SPI_COM_TX to high
// impedance to not hinder other subordinate units from communicating.
// When chip select goes low, connect push-pull and SPI function to pin.
void gpio_callback(uint gpio, uint32_t events) {
    int cr1 = spi_get_hw(SPI_COM_PORT)->cr1;
    if (!gpio_get(gpio)) {
        gpio_set_function(SPI_COM_TX, GPIO_FUNC_SPI);
        timerval1 = time_us_64();
    } else {
        gpio_set_function(SPI_COM_TX, GPIO_FUNC_SIO);
        timerval2 = time_us_64();
    }
}

// inline function to check if spi transmit buffer is empty
static inline bool spi_tx_is_empty(const spi_inst_t *spi) {
    return (spi_get_const_hw(spi)->sr & 0x01);
}

int main() {
    // Enable UART so we can print
    stdio_init_all();

    // Enable SPI 0 at 1 MHz and connect to GPIOs
    spi_init(SPI_COM_PORT, 100 * 1000);
    spi_set_format(SPI_COM_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    spi_set_slave(SPI_COM_PORT, true);
    gpio_set_function(SPI_COM_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_TX, GPIO_FUNC_SIO);
    gpio_set_function(SPI_COM_CS, GPIO_FUNC_SPI);

    // Init GPIO pin for status LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    HX71708_init();

    // Enable IRQ and set callback for SPI chip select pin to fix SPI communication
    gpio_set_irq_enabled_with_callback(SPI_COM_CS, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Write null terminator to in buffer for easier parsing
    in_buf[4] = '\0';
    
    // Read calibration data from flash.
    // If flash portion is not initialized, set calibration value to 1.000
    read_calib_data();
    if(calib_val[0] == 255){
        calib_val[0] = '1';
        calib_val[1] = '0';
        calib_val[2] = '0';
        calib_val[3] = '0';
        save_calib_data();
    }
    // calculate integer calibration value from string stored in flash
    for (int i = 0; i < 4; i++) {
        calib_int += (calib_val[i] - 48) * pow(10, (3 - i));
    }

    printf("Start!\n");

    while (1) {
        time_now = time_us_64() / 1000;
        if (!spi_is_busy(SPI_COM_PORT)) {
            // check if SPI transmit buffer is empty
            // if so, write latest values to transmit buffer
            if (spi_tx_is_empty(SPI_COM_PORT)) {
                spi_get_hw(SPI_COM_PORT)->dr = out_buf[0];
                spi_get_hw(SPI_COM_PORT)->dr = out_buf[1];
                spi_get_hw(SPI_COM_PORT)->dr = out_buf[2];
                spi_get_hw(SPI_COM_PORT)->dr = out_buf[3];
            }
            // check if new data is in SPI reveive buffer
            // if so, write data from buffer to array
            if (spi_is_readable(SPI_COM_PORT)) {
                in_buf[0] = spi_get_hw(SPI_COM_PORT)->dr;
                in_buf[1] = spi_get_hw(SPI_COM_PORT)->dr;
                in_buf[2] = spi_get_hw(SPI_COM_PORT)->dr;
                in_buf[3] = spi_get_hw(SPI_COM_PORT)->dr;
            }
            // parse string from reveive buffer
            if (strcmp(in_buf, "TARE") == 0) {
                hx1.offset_counter = 0;
                hx1.offset = 0;
                hx2.offset_counter = 0;
                hx2.offset = 0;
            }
        }

        // general scheduler
        if (time_now != time_last) {
            // check if both HX71708 chips are ready to provide new data
            if (!gpio_get(HX1_DOUT) && !gpio_get(HX2_DOUT)) {
                hx1_data = HX71708_read(&hx1);
                hx2_data = HX71708_read(&hx2);
                gpio_xor_mask(1 << LED_PIN);

                // add up data from both chips and apply calibration data
                int32_t result = ~(((hx1_data + hx2_data) * calib_int) / 1000);

                // write conversion result to output buffer
                out_buf[0] = (uint8_t)((result >> 24) & 0xFF);
                out_buf[1] = (uint8_t)((result >> 16) & 0xFF);
                out_buf[2] = (uint8_t)((result >> 8) & 0xFF);
                out_buf[3] = (uint8_t)(result & 0xFF);
            }
            if ((time_now % 100) == 0) {
                // check if new input is in serial terminal input buffer
                char c = getchar_timeout_us(100);
                switch (console_mode) {
                case debug_out:
                    printf("%c%c%c%c", 0x1B, 0x5B, 0x32, 0x4A);
                    printf("Press \"k\" to enter Calibration Mode.\n");
                    printf("HX1: %.1f\t%d\t%d\n", (hx1_data / 26.7), hx1.offset, hx1.sample_stats.sample_time);
                    printf("HX2: %.1f\t%d\t%d\n", (hx2_data / 26.7), hx2.offset, hx2.sample_stats.sample_time);
                    printf("Total: %.1f\n", ((hx1_data + hx2_data) / 26.7));
                    printf("Timerdiff: %d\n", timerval2 - timerval1);
                    printf("Calibration Factor: %c.%c%c%c\n", calib_val[0], calib_val[1], calib_val[2], calib_val[3]);
                    printf("In Buffer: ");
                    printf(in_buf);
                    // check if "k" key was pressed, change to calibration input mode
                    if (c == 'k') {
                        calib_counter = 0;
                        for (int i=0; i < 4; i++) {
                            calib_val[i] = 'x';
                        }
                        console_mode = calib_in;
                    }
                    break;

                case calib_in:
                    printf("%c%c%c%c", 0x1B, 0x5B, 0x32, 0x4A);
                    printf("Calibration Mode:\n");
                    printf("Enter calibration factor as integer in the format x.xx.\n");
                    printf("Examples 1.051 or 0.964.\n");
                    printf("Store value by pressing enter.\n");
                    printf("New Calibration Factor: %c.%c%c%c\n", calib_val[0], calib_val[1], calib_val[2], calib_val[3]);
                    if (calib_counter < 4) {
                        if ((c >= '0') && (c <= '9')) {
                            calib_val[calib_counter] = c;
                            calib_counter++;
                        }
                    }
                    // check if "backspace" was pressed, delete latest value from input string
                    if ((c == 8) && (calib_counter > 0)) {
                        calib_val[calib_counter - 1] = (char)'x';
                        calib_counter--;
                    }
                    // check if "enter" was pressed, save data to flash and return to 
                    if (c == 13) {
                        // check if user input a full number
                        // save new data or load old data
                        if (calib_counter == 4){
                            save_calib_data();
                            calib_int = 0;
                            for (int i = 0; i < 4; i++) {
                                calib_int += (calib_val[i] - 48) * pow(10, (3 - i));
                            }
                        }
                        else{
                            read_calib_data();
                        }
                        console_mode = debug_out;
                    }
                    break;

                default:
                    break;
                }
            }
            time_last = time_now;
        }

    }
}

// function to wirt and read to and from flash
// taken and modified from https://forums.raspberrypi.com//viewtopic.php?f=144&t=310821
void save_calib_data() {
    uint8_t *myDataAsBytes = (uint8_t *)&calib_val;
    int myDataSize = sizeof(calib_val);

    int writeSize = (myDataSize / FLASH_PAGE_SIZE) + 1; // how many flash pages we're gonna need to write
    int sectorCount = ((writeSize * FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE) + 1; // how many flash sectors we're gonna need to erase

    printf("Programming flash target region...\n");

    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE * sectorCount);
    flash_range_program(FLASH_TARGET_OFFSET, myDataAsBytes, FLASH_PAGE_SIZE * writeSize);
    restore_interrupts(interrupts);

    printf("Done.\n");
}

void read_calib_data() {
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(calib_val, flash_target_contents, sizeof(calib_val));
}