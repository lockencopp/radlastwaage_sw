// Copyright (c) 2021 Michael Stoops. All rights reserved.
// Portions copyright (c) 2021 Raspberry Pi (Trading) Ltd.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
// following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
//    disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//    following disclaimer in the documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
//    products derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// SPDX-License-Identifier: BSD-3-Clause
//
// Example of an SPI bus slave using the PL022 SPI interface

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#define BUF_LEN         0x04

#define HX1_DOUT 0
#define HX1_SCK 1
#define HX2_DOUT 2
#define HX2_SCK 3

#define SPI_COM_PORT spi0
#define SPI_COM_RX 16
#define SPI_COM_TX 19
#define SPI_COM_SCK 18
#define SPI_COM_CS 17

int read_flag = 0;
int timerval1 = 0;
int timerval2 = 0;

const uint LED_PIN = 25;
uint32_t timeNow = 0;
uint32_t timeLast = 0;
int32_t hx1_data = 0;
int32_t hx2_data = 0;
uint32_t sampleNow = 0;
uint32_t sampleLast = 0;
uint32_t sampleNow2 = 0;
uint32_t sampleLast2 = 0;
uint32_t sampleTime = 0;
uint32_t sampleTime2 = 0;
int offset1 = 0;
int offset2 = 0;
int32_t offsetCnt = 0;

void printbuf(uint8_t buf[], size_t len) {
    int i;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 15)
            printf("%02x\n", buf[i]);
        else
            printf("%02x ", buf[i]);
    }

    // append trailing newline if there isn't one
    if (i % 16) {
        putchar('\n');
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    // Put the GPIO event(s) that just happened into event_str
    // so we can print it
    if (gpio_get(gpio)) {
        read_flag = 1;
        timerval2 = time_us_64();
        printf("1\n");
    } else {
        timerval1 = time_us_64();
        printf("2\n");
    }
}

inline void delay_asm(int clocks) {
    for (int i = 0; i < clocks; i++) {
        __asm__("NOP");
    }
}

int HX711_init() {
    gpio_init(HX1_DOUT);
    gpio_init(HX1_SCK);
    gpio_init(HX2_DOUT);
    gpio_init(HX2_SCK);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(HX1_DOUT, GPIO_IN);
    gpio_set_dir(HX2_DOUT, GPIO_IN);
    gpio_set_dir(HX1_SCK, GPIO_OUT);
    gpio_set_dir(HX2_SCK, GPIO_OUT);

    gpio_put(HX1_SCK, 0);
    gpio_put(HX2_SCK, 0);
    gpio_put(LED_PIN, 0);
}

int HX711_read(uint HX_DOUT, uint HX_SCK, int *offset) {
    int hx_data = 0;
    int j = 23;

    for (int i = 0; i < 25; i++) {
        gpio_put(HX_SCK, 1);

        if (j >= 0)
            hx_data |= (gpio_get(HX_DOUT) << j);
        else
            delay_asm(2);

        j--;

        gpio_put(HX_SCK, 0);
        delay_asm(3);
    }
    if (hx_data > 0x7fffff) {
        hx_data -= 0x1000000;
    }
    if ((offsetCnt <= 50)) {
        return 0;
    } else if (*offset == 0) {
        *offset = hx_data;
    }
    return hx_data - *offset;
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
    gpio_set_function(SPI_COM_TX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_CS, GPIO_FUNC_SPI);
    gpio_set_irq_enabled_with_callback(SPI_COM_CS, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    //gpio_init(SPI_COM_TX);
    //gpio_set_dir(SPI_COM_TX,GPIO_OUT);
    //gpio_put(SPI_COM_TX,1);
    //gpio_pull_up(SPI_COM_TX);

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i) {
        // bit-inverted from i. The values should be: {0xff, 0xfe, 0xfd...}
        out_buf[i] = ~i;
    }

    HX711_init();

    printf("Start!\n");

    hx1_data = HX711_read(HX1_DOUT, HX1_SCK, &offset1);
    hx2_data = HX711_read(HX2_DOUT, HX2_SCK, &offset2);

    while (1) {
        // Write the output buffer to MISO, and at the same time read from MOSI.
        //spi_write_read_blocking(spi_default, out_buf, in_buf, BUF_LEN);

        if (read_flag == 1) {
            int32_t result = (hx1_data + hx2_data);
            //int32_t result = time_us_32();

            out_buf[0] = (uint8_t)((result >> 24) & 0xFF);
            out_buf[1] = (uint8_t)((result >> 16) & 0xFF);
            out_buf[2] = (uint8_t)((result >> 8) & 0xFF);
            out_buf[3] = (uint8_t)(result & 0xFF);

            //gpio_put(SPI_COM_TX,0);

            //gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

            spi_write_read_blocking(SPI_COM_PORT, out_buf, in_buf, BUF_LEN);
            /*
            printf("OUT:\n");
            printf("%3.1f\n", result/26400.0f);
            printbuf(out_buf, BUF_LEN);
            printf("IN:\n");
            printbuf(in_buf, BUF_LEN);
            printf("Timerdiff: %d", timerval2-timerval1);

            */
            read_flag = 0;

            //gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SIO);
            //gpio_put(SPI_COM_TX,1);
        }

        timeNow = time_us_32() / 1000;


        if (timeNow != timeLast) {
            if ((timeNow % 10) == 0) {
                offsetCnt++;
                if (!gpio_get(HX1_DOUT) && !gpio_get(HX2_DOUT)) {
                    sampleNow=timeNow;
                    sampleTime=(sampleNow - sampleLast) / 1000;
                    sampleLast=sampleNow;
                    hx1_data = HX711_read(HX1_DOUT, HX1_SCK, &offset1);
                    hx2_data = HX711_read(HX2_DOUT, HX2_SCK, &offset2);
                }
            }
            if ((timeNow % 100) == 0) {
                int32_t result1 = hx1_data;
                int32_t result2 = hx2_data;

                printf("%c%c%c%c", 0x1B, 0x5B, 0x32, 0x4A);
                printf("HX1: %.1f\n", (hx1_data / 13.2));
                printf("HX2: %.1f\n", (hx2_data / 13.2));
                printf("Total: %.1f\t%d\n", ((hx1_data + hx2_data) / 13.2), sampleTime);
                printf("Timerdiff: %d\n", timerval2 - timerval1);

                gpio_xor_mask(1 << LED_PIN);
            }
            timeLast = timeNow;
        }
    }
}
