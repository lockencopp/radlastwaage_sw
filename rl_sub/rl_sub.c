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

#include "hx71708.h"

#define BUF_LEN         0x04

#define LED_PIN         25
#define SPI_COM_PORT    spi0
#define SPI_COM_RX      16
#define SPI_COM_TX      19
#define SPI_COM_SCK     18
#define SPI_COM_CS      17

volatile int read_flag = 0;
volatile int timerval1 = 0;
volatile int timerval2 = 0;

HX71708_t hx1 = { .dout = HX1_DOUT, .sck = HX1_SCK, .offset = 0, .offset_counter = 0 };
HX71708_t hx2 = { .dout = HX2_DOUT, .sck = HX2_SCK, .offset = 0, .offset_counter = 0 };

uint32_t time_now = 0;
uint32_t time_last = 0;
int32_t hx1_data = 0;
int32_t hx2_data = 0;

void gpio_callback(uint gpio, uint32_t events) {
    // Put the GPIO event(s) that just happened into event_str
    // so we can print it
    if (gpio_get(gpio)) {
        read_flag = 1;
        timerval2 = time_us_64();
    } else {
        timerval1 = time_us_64();
    }
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

    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    HX71708_init();

    printf("Start!\n");

    while (1) {
        if (read_flag == 1) {
            int32_t result = ~(hx1_data + hx2_data);

            out_buf[0] = (uint8_t)((result >> 24) & 0xFF);
            out_buf[1] = (uint8_t)((result >> 16) & 0xFF);
            out_buf[2] = (uint8_t)((result >> 8) & 0xFF);
            out_buf[3] = (uint8_t)(result & 0xFF);

            spi_write_read_blocking(SPI_COM_PORT, out_buf, in_buf, BUF_LEN);
            read_flag = 0;
        }

        time_now = time_us_64() / 1000;

        if (time_now != time_last) {
            
            if (!gpio_get(HX1_DOUT) && !gpio_get(HX2_DOUT)) {
                hx1_data = HX71708_read(&hx1);
                hx2_data = HX71708_read(&hx2);
            }
            if ((time_now % 100) == 0) {

                printf("%c%c%c%c", 0x1B, 0x5B, 0x32, 0x4A);
                printf("HX1: %.1f\t%d\n", (hx1_data / 13.2), hx1.sample_stats.sample_time);
                printf("HX2: %.1f\t%d\n", (hx2_data / 13.2), hx2.sample_stats.sample_time);
                printf("Total: %.1f\n", ((hx1_data + hx2_data) / 13.2));
                printf("Timerdiff: %d\n", timerval2 - timerval1);

                gpio_xor_mask(1 << LED_PIN);
            }
            time_last = time_now;
        }
    }
}
