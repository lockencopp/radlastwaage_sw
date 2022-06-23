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
// Example of an SPI bus master using the PL022 SPI interface

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hw.h"
#include "tst_funcs.h"
#include "ST7735_TFT.h"

#define BUF_LEN 0x04

#define LED0 29
#define LED1 28
#define LED2 27
#define LED3 26

#define SPI_COM_PORT spi0
#define SPI_COM_RX 16
#define SPI_COM_TX 19
#define SPI_COM_SCK 18
#define SPI_COM_CS0 18
#define SPI_COM_CS1 18
#define SPI_COM_CS2 18
#define SPI_COM_CS3 18

uint32_t timeNow = 0;
uint32_t timeLast = 0;

void printbuf(uint8_t buf[], size_t len)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        if (i % 16 == 15)
            printf("%02x\n", buf[i]);
        else
            printf("%02x ", buf[i]);
    }

    // append trailing newline if there isn't one
    if (i % 16)
    {
        putchar('\n');
    }
}

void init_hw()
{
    stdio_init_all();

    // Enable SPI 0 at 10kHz and connect to GPIOs
    spi_init(spi_default, 10000);
    spi_set_format(SPI_COM_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(SPI_COM_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_TX, GPIO_FUNC_SPI);

    gpio_init(SPI_COM_CS0);
    gpio_set_dir(SPI_COM_CS0, GPIO_OUT);
    gpio_put(SPI_COM_CS0, 1);

    gpio_init(SPI_COM_CS1);
    gpio_set_dir(SPI_COM_CS1, GPIO_OUT);
    gpio_put(SPI_COM_CS1, 1);

    gpio_init(SPI_COM_CS2);
    gpio_set_dir(SPI_COM_CS2, GPIO_OUT);
    gpio_put(SPI_COM_CS2, 1);

    gpio_init(SPI_COM_CS3);
    gpio_set_dir(SPI_COM_CS3, GPIO_OUT);
    gpio_put(SPI_COM_CS3, 1);

    spi_init(SPI_TFT_PORT, 1000000); // SPI with 1Mhz
    gpio_set_function(SPI_TFT_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_TX, GPIO_FUNC_SPI);

    gpio_init(SPI_TFT_CS);
    gpio_set_dir(SPI_TFT_CS, GPIO_OUT);
    gpio_put(SPI_TFT_CS, 1); // Chip select is active-low

    gpio_init(SPI_TFT_DC);
    gpio_set_dir(SPI_TFT_DC, GPIO_OUT);
    gpio_put(SPI_TFT_DC, 0); // Chip select is active-low

    gpio_init(SPI_TFT_RST);
    gpio_set_dir(SPI_TFT_RST, GPIO_OUT);
    gpio_put(SPI_TFT_RST, 0);

    gpio_init(LED0);
    gpio_set_dir(LED0, GPIO_OUT);

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
}

int main()
{
    // Enable UART so we can print
    stdio_init_all();

    init_hw();

    printf("SPI master example\n");

    // gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    // gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    // gpio_put(PICO_DEFAULT_SPI_CSN_PIN,1);

    // Make the SPI pins available to picotool
    // bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i)
    {
        out_buf[i] = i;
    }

    printf("SPI master says: The following buffer will be written to MOSI endlessly:\n");
    printbuf(out_buf, BUF_LEN);

    while (1)
    {
        timeNow = time_us_32();

        if (timeNow != timeLast)
        {
            if ((timeNow % 10000) == 0)
            {
            }
            if ((timeNow % 100000) == 0)
            {
            }
            if ((timeNow % 1000000) == 0)
            {
                // Write the output buffer to MOSI, and at the same time read from MISO.
                gpio_put(SPI_COM_CS0,0);
                
                while(gpio_get(SPI_COM_RX) == 0)
                {
                    __asm__("NOP");
                }
                spi_write_read_blocking(spi_default, out_buf, in_buf, BUF_LEN);
    
                gpio_put(SPI_COM_CS0,1);
                // Write to stdio whatever came in on the MISO line.
                printbuf(in_buf, BUF_LEN);

                // Sleep for ten seconds so you get a chance to read the output.
                gpio_xor_mask(1 << LED0);
            }
        }
    }
}
