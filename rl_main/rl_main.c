// Author: Christoph Deussen
//
// Code for main, handheld headunit of wheel load system.
// Reading from subordinate devices and displays data on LCD.
// Subordinates can be tared via button input.
// Data can be displayed in KGs or in percentage of total weight.

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hw.h"
#include "tst_funcs.h"
#include "ST7735_TFT.h"
#include "display_helpers.h"

#define BUF_LEN         4
#define NUM_MODES       3

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

#define KG_CALIB_FACTOR 26700.0f

typedef struct Pin {
    uint pin_num;
    bool direction;
    bool polarity;
} Pin;

SubModule sub_modules[NUM_SUBS] = {
    {.led_pin = LED0, .cs_pin = SPI_COM_CS0, .result = 0.0f},
    {.led_pin = LED1, .cs_pin = SPI_COM_CS1, .result = 0.0f},
    {.led_pin = LED2, .cs_pin = SPI_COM_CS2, .result = 0.0f},
    {.led_pin = LED3, .cs_pin = SPI_COM_CS3, .result = 0.0f}
};

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

uint32_t time_now       = 0;
uint32_t time_last      = 0;

bool btn_now            = 0;
bool btn_last           = 0;
uint btn_counter        = 0;

Mode mode_now           = 0;
Mode mode_next          = 0;
uint mode_switch_cnt    = 0;

uint tare_flag = 0;
uint8_t out_buf[BUF_LEN] = { 'N', 'O', 'N', 'E' };
char disp_buf[10];

void init_pins();
void init_hw();
void init_tft();
void read_sub(uint sub_num);
void scan_button();
void print_KG();
void print_percent();
void print_cross();

int main() {
    // Enable UART so we can print
    stdio_init_all();

    init_hw();

    init_tft();

    while (1) {
        time_now = time_us_32() / 1000;

        if (time_now != time_last) {
            if ((time_now % 100) == 0) {
                scan_button();
            }
            if ((time_now % 200) == 0) {
                if (tare_flag == 1) {
                    strncpy(out_buf, "TARE", 4);
                    /*
                    out_buf[0] = 'T';
                    out_buf[1] = 'A';
                    out_buf[2] = 'R';
                    out_buf[3] = 'E';
                    */
                    tare_flag = 2;
                } else if (tare_flag == 2) {
                    strncpy(out_buf, "NONE", 4);
                    /*
                    out_buf[0] = 'N';
                    out_buf[1] = 'O';
                    out_buf[2] = 'N';
                    out_buf[3] = 'E';
                    */
                    tare_flag = 0;
                }
            }
            if ((time_now % 200) == 1) {
                read_sub(0);
            }
            if ((time_now % 200) == 3) {
                read_sub(1);
            }
            if ((time_now % 200) == 5) {
                read_sub(2);
            }
            if ((time_now % 200) == 7) {
                read_sub(3);
            }
            if ((time_now % 200) == 9) {
                switch (mode_now) {
                case kKilogram:
                    if (mode_next == kPercent) {
                        mode_switch_cnt++;
                        if (mode_switch_cnt == 1) {
                            //draw rectangle to indicate switch to percent mode
                            draw_mode_indicator_text(mode_next);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            clear_mode_indicator_text(mode_next);
                            //move the mode indication bar to the correct position for next mode
                            set_mode_indicator_bar(mode_next);
                            mode_now = kPercent;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_KG(sub_modules, disp_buf);
                    }
                    break;

                case kPercent:
                    if (mode_next == kCross) {
                        mode_switch_cnt++;
                        if (mode_switch_cnt == 1) {
                            //draw rectangle to indicate switch to cross mode
                            draw_mode_indicator_text(mode_next);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            clear_mode_indicator_text(mode_next);
                            //move the mode indication bar to the correct position for next mode
                            set_mode_indicator_bar(mode_next);
                            print_cross_numbers(disp_buf);
                            mode_now = kCross;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_percent(sub_modules, disp_buf);
                    }
                    break;

                case kCross:
                    if (mode_next == kKilogram) {
                        mode_switch_cnt++;
                        if (mode_switch_cnt == 1) {
                            //draw rectangle to indicate switch to kg mode
                            draw_mode_indicator_text(mode_next);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            clear_mode_indicator_text(mode_next);
                            //move the mode indication bar to the correct position for next mode
                            set_mode_indicator_bar(mode_next);
                            print_normal_numbers(disp_buf);
                            mode_now = kKilogram;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_cross(sub_modules, disp_buf);
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

    spi_init(SPI_TFT_PORT, 10 * 1000 * 1000); // SPI with 10Mhz
    gpio_set_function(SPI_TFT_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_TX, GPIO_FUNC_SPI);

    init_pins();

    btn_last = gpio_get(BTN_IN);
}

void init_tft() {
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
    drawFastVLine(0, 0, 43, ST7735_WHITE);

    print_normal_numbers(disp_buf);
}

void read_sub(uint sub_num) {
    uint8_t in_buf[BUF_LEN];

    assert(sub_num < 4);

    gpio_put(sub_modules[sub_num].cs_pin, 0);

    sleep_us(100);

    spi_write_read_blocking(SPI_COM_PORT, out_buf, in_buf, BUF_LEN);

    sleep_us(100);

    gpio_put(sub_modules[sub_num].cs_pin, 1);

    int temp_result_int = (in_buf[0] << 24) | (in_buf[1] << 16) | (in_buf[2] << 8) | in_buf[3];

    if (temp_result_int == 0) {
        gpio_put(sub_modules[sub_num].led_pin, 0);
        sub_modules[sub_num].result = 0.0f;
    } else {
        gpio_xor_mask(1 << sub_modules[sub_num].led_pin);
        sub_modules[sub_num].result = (float)((~temp_result_int) / KG_CALIB_FACTOR);
        sub_modules[sub_num].oor_flag = false;
    }

    if ((sub_modules[sub_num].result > 240.0) || (-100 > sub_modules[sub_num].result)) {
        sub_modules[sub_num].result = 0.0f;
        sub_modules[sub_num].oor_flag = true;
    }
}

void scan_button() {
    btn_now = gpio_get(BTN_IN);
    if (btn_now != btn_last) {
        if (!btn_now) {             //button pressed
            btn_counter++;
        } else {                    //button released
            if (btn_counter > 1) {
                btn_counter = 0;
                mode_next = mode_now + 1;
                if (mode_next >= NUM_MODES) {
                    mode_next = 0;
                }
            }
        }
    } else {
        if (!btn_now) {
            if (btn_counter > 0) {
                btn_counter++;
            }
            if (btn_counter > 20) {
                tare_flag = 1;
                btn_counter = 0;
            }
        }
    }
    btn_last = btn_now;
}
