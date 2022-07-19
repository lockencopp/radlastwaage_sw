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

#define SPI_COM_PORT    spi0
#define SPI_COM_RX      16
#define SPI_COM_TX      19
#define SPI_COM_SCK     18

#define NUM_PINS        12
#define LED0            29
#define LED1            28
#define LED2            27
#define LED3            26
#define SPI_COM_CS0     25
#define SPI_COM_CS1     24
#define SPI_COM_CS2     23
#define SPI_COM_CS3     22
#define BTN_IN          14

#define MAX_PADDING     3

#define KG_CALIB_FACTOR 13200.0f

#define NUM_MODES       2

typedef struct {
    uint pin_num;
    bool direction;
    bool polarity;
} Pin;

typedef enum {
    kKilogram   = 0,
    kPercent    = 1
} Mode;

const int hxChipSelect[4] = { SPI_COM_CS0, SPI_COM_CS1, SPI_COM_CS2, SPI_COM_CS3 };
const Pin pins[NUM_PINS] = {
    {.pin_num = LED0, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = LED1, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = LED2, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = LED3, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = SPI_COM_CS0, .direction = GPIO_OUT, .polarity = 1},
    {.pin_num = SPI_COM_CS1, .direction = GPIO_OUT, .polarity = 1},
    {.pin_num = SPI_COM_CS2, .direction = GPIO_OUT, .polarity = 1},
    {.pin_num = SPI_COM_CS3, .direction = GPIO_OUT, .polarity = 1},
    {.pin_num = SPI_TFT_CS, .direction = GPIO_OUT, .polarity = 1},
    {.pin_num = SPI_TFT_DC, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = SPI_TFT_RST, .direction = GPIO_OUT, .polarity = 0},
    {.pin_num = BTN_IN, .direction = GPIO_IN, .polarity = 0}
};

uint32_t time_now = 0;
uint32_t time_last = 0;

float result_sub0 = 0.0f;
float result_sub1 = 0.0f;
float result_sub2 = 0.0f;
float result_sub3 = 0.0f;

bool btn_now = 0;
bool btn_last = 0;
uint btn_counter = 0;

Mode mode_now;
Mode mode_next;
uint mode_switch_counter = 0;

uint tare_flag = 0;
uint8_t out_buf[BUF_LEN] = { 'N', 'O', 'N', 'E' };

// can be used to pad number outputs e.g. sprintf(disp_buf, "2: %*.1f", padLeftCalc(result_sub1), result_sub1);
int padLeftCalc(float input) {
    int padding = MAX_PADDING;
    if (input <= -100) {
        padding = padding - 3;
    } else if ((input >= 100) || (input <= -10)) {
        padding = padding - 2;
    } else if ((input >= 10) || (input < 0)) {
        padding = padding - 1;
    } else if (input >= 0.0) {
        padding = MAX_PADDING;
    }
    return padding;
}

void init_pins() {
    for (int i = 0; i < NUM_PINS; i++) {
        Pin pin = pins[i];
        gpio_init(pin.pin_num);
        gpio_set_dir(pin.pin_num, pin.direction);
        if (pin.direction == GPIO_OUT) {
            gpio_put(pin.pin_num, pin.polarity);
        }
    }
}

void init_hw() {
    stdio_init_all();

    // Enable SPI 0 at 10kHz and connect to GPIOs

    spi_init(SPI_COM_PORT, 100 * 1000);
    spi_set_format(SPI_COM_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(SPI_COM_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_COM_TX, GPIO_FUNC_SPI);

    spi_init(SPI_TFT_PORT, 10000000); // SPI with 1Mhz
    gpio_set_function(SPI_TFT_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_TX, GPIO_FUNC_SPI);

    init_pins();

    btn_last = gpio_get(BTN_IN);
}

float readSub(uint sub_num) {
    uint8_t in_buf[BUF_LEN];

    assert(sub_num < 4);

    gpio_put(hxChipSelect[sub_num], 0);

    sleep_us(100);
    spi_write_read_blocking(SPI_COM_PORT, out_buf, in_buf, BUF_LEN);

    sleep_us(100);

    gpio_put(hxChipSelect[sub_num], 1);

    int result = (in_buf[0] << 24) | (in_buf[1] << 16) | (in_buf[2] << 8) | in_buf[3];

    if (result == 0) {
        return -1000;
    } else {
        return ~result / KG_CALIB_FACTOR;
    }
}

char disp_buf[10];

int main() {
    // Enable UART so we can print
    stdio_init_all();

    init_hw();

    TFT_RedTab_Initialize();

    setTextWrap(true);
    TEST_DELAY1();
    fillScreen(ST7735_BLACK);
    setRotation(1);
    tft_width = 160;
    tft_height = 128;

    drawFastHLine(10, 30, 100, ST7735_WHITE);
    drawFastHLine(10, 62, 100, ST7735_WHITE);
    drawFastHLine(10, 94, 100, ST7735_WHITE);
    drawFastVLine(0, 0, 64, ST7735_WHITE);

    sprintf(disp_buf, "1:");
    drawText(5, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "2:");
    drawText(5, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "3:");
    drawText(5, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "4:");
    drawText(5, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);

    while (1) {
        time_now = time_us_32() / 1000;

        if (time_now != time_last) {
            if ((time_now % 100) == 0) {
                btn_now = gpio_get(BTN_IN);
                if (btn_now != btn_last) {
                    if (!btn_now) {             //button pressed
                        printf("Pressed\n");
                        btn_counter++;
                    } else {                    //button released
                        if (btn_counter > 1) {
                            printf("Released, %d\n", btn_counter);
                            btn_counter = 0;
                            mode_next = mode_now + 1;
                            if (mode_next >= NUM_MODES) {
                                printf("resetMode\n");
                                mode_next = 0;
                            }
                        }
                    }
                } else {
                    if (!btn_now) {
                        if (btn_counter > 0) {
                            printf("holding %d\n", btn_counter);
                            btn_counter++;
                        }
                        if (btn_counter > 20) {
                            printf("holding action");
                            tare_flag = 1;
                            btn_counter = 0;
                        }
                    }
                }
                btn_last = btn_now;
            }
            if ((time_now % 200) == 0) {
                if (tare_flag == 1) {
                    out_buf[0] = 'T';
                    out_buf[1] = 'A';
                    out_buf[2] = 'R';
                    out_buf[3] = 'E';
                    tare_flag = 2;
                } else if (tare_flag == 2) {
                    out_buf[0] = 'N';
                    out_buf[1] = 'O';
                    out_buf[2] = 'N';
                    out_buf[3] = 'E';
                    tare_flag = 0;
                }
            }
            if ((time_now % 200) == 1) {
                float temp_result_sub0 = readSub(0);
                if (temp_result_sub0 == -1000) {
                    gpio_put(LED0, 0);
                } else if ((temp_result_sub0 < 1000) && (temp_result_sub0 > -999)) {
                    result_sub0 = temp_result_sub0;
                    gpio_xor_mask(1 << LED0);
                }
            }
            if ((time_now % 200) == 3) {
                float temp_result_sub1 = readSub(1);
                if (temp_result_sub1 == -1000) {
                    gpio_put(LED1, 0);
                } else if ((temp_result_sub1 < 1000) && (temp_result_sub1 > -999)) {
                    result_sub1 = temp_result_sub1;
                    gpio_xor_mask(1 << LED1);
                }
            }
            if ((time_now % 200) == 5) {
                float temp_result_sub2 = readSub(2);
                if (temp_result_sub2 == -1000) {
                    gpio_put(LED2, 0);
                } else if ((temp_result_sub2 < 1000) && (temp_result_sub2 > -999)) {
                    result_sub2 = temp_result_sub2;
                    gpio_xor_mask(1 << LED2);
                }
            }
            if ((time_now % 200) == 7) {
                float temp_result_sub3 = readSub(3);
                if (temp_result_sub3 == -1000) {
                    gpio_put(LED3, 0);
                } else if ((temp_result_sub3 < 1000) && (temp_result_sub3 > -999)) {
                    result_sub3 = temp_result_sub3;
                    gpio_xor_mask(1 << LED3);
                }
            }
            if ((time_now % 200) == 9) {
                // Write the output buffer to MOSI, and at the same time read from MISO.

                // Write to stdio whatever came in on the MISO line.
                printf("MNow: %d\tMNxt: %d\tMCnt: %d\n", mode_now, mode_next, mode_switch_counter);

                switch (mode_now) {
                case kKilogram:
                    printf("KiloCase\n");
                    if (mode_next == kPercent) {
                        mode_switch_counter++;
                        if (mode_switch_counter == 1) {
                            fillRect(62, 40, 36, 48, ST7735_BLACK);
                            drawRectWH(62, 40, 36, 48, ST7735_WHITE);
                            drawText(65, 43, "%", ST7735_WHITE, ST7735_BLACK, 6);
                        } else if (mode_switch_counter > 10) {
                            drawRectWH(62, 40, 36, 48, ST7735_BLACK);
                            drawText(65, 43, " ", ST7735_WHITE, ST7735_BLACK, 6);
                            drawFastHLine(10, 30, 100, ST7735_WHITE);
                            drawFastHLine(10, 62, 100, ST7735_WHITE);
                            drawFastHLine(10, 94, 100, ST7735_WHITE);
                            drawFastVLine(0, 0, 64, ST7735_BLACK);
                            drawFastVLine(0, 63, 64, ST7735_WHITE);
                            mode_now = kPercent;
                            mode_switch_counter = 0;
                        }
                    } else {
                        sprintf(disp_buf, "%*s%.1f", padLeftCalc(result_sub0), "", result_sub0);
                        drawText(39, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                        sprintf(disp_buf, "%*s%.1f", padLeftCalc(result_sub1), "", result_sub1);
                        drawText(39, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                        sprintf(disp_buf, "%*s%.1f", padLeftCalc(result_sub2), "", result_sub2);
                        drawText(39, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                        sprintf(disp_buf, "%*s%.1f", padLeftCalc(result_sub3), "", result_sub3);
                        drawText(39, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                    }
                    break;

                case kPercent:

                    printf("percentCase\n");

                    if (mode_next == kKilogram) {

                        mode_switch_counter++;
                        if (mode_switch_counter == 1) {
                            fillRect(44, 40, 72, 48, ST7735_BLACK);
                            drawRectWH(44, 40, 72, 48, ST7735_WHITE);
                            drawText(47, 43, "kg", ST7735_WHITE, ST7735_BLACK, 6);

                        } else if (mode_switch_counter > 10) {
                            drawRectWH(44, 40, 72, 48, ST7735_BLACK);
                            drawText(47, 43, "  ", ST7735_WHITE, ST7735_BLACK, 6);
                            drawFastHLine(10, 30, 100, ST7735_WHITE);
                            drawFastHLine(10, 62, 100, ST7735_WHITE);
                            drawFastHLine(10, 94, 100, ST7735_WHITE);
                            drawFastVLine(0, 0, 64, ST7735_WHITE);
                            drawFastVLine(0, 63, 64, ST7735_BLACK);
                            mode_now = kKilogram;
                            mode_switch_counter = 0;
                        }
                    } else {
                        float result_sum = result_sub0 + result_sub1 + result_sub2 + result_sub3;
                        if (result_sum == 0) {
                            drawText(64, 4, "   NA", ST7735_WHITE, ST7735_BLACK, 3);
                            drawText(64, 36, "   NA", ST7735_WHITE, ST7735_BLACK, 3);
                            drawText(64, 68, "   NA", ST7735_WHITE, ST7735_BLACK, 3);
                            drawText(64, 100, "   NA", ST7735_WHITE, ST7735_BLACK, 3);
                        } else {
                            int result_sub0_percent = (int)((result_sub0 * 100.0) / result_sum);
                            int result_sub1_percent = (int)((result_sub1 * 100.0) / result_sum);
                            int result_sub2_percent = (int)((result_sub2 * 100.0) / result_sum);
                            int result_sub3_percent = (int)((result_sub3 * 100.0) / result_sum);
                            printf("%d\t%.2f\n", padLeftCalc(result_sub0_percent), result_sub0_percent);
                            sprintf(disp_buf, "%*s%d", padLeftCalc(result_sub0_percent) + 2, "", result_sub0_percent); //padLeftCalc(result_sub0/result_sum)
                            drawText(39, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                            sprintf(disp_buf, "%*s%d", padLeftCalc(result_sub1_percent) + 2, "", result_sub1_percent);
                            drawText(39, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                            sprintf(disp_buf, "%*s%d", padLeftCalc(result_sub2_percent) + 2, "", result_sub2_percent);
                            drawText(39, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                            sprintf(disp_buf, "%*s%d", padLeftCalc(result_sub3_percent) + 2, "", result_sub3_percent);
                            drawText(39, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
                        }
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
