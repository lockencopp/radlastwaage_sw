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

#define BUF_LEN         4
#define NUM_SUBS        4
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

#define MAX_PADDING     3

#define KG_CALIB_FACTOR 26700.0f

typedef struct Pin {
    uint pin_num;
    bool direction;
    bool polarity;
} Pin;

typedef enum Mode {
    kKilogram   = 0,
    kPercent    = 1,
    kCross      = 2
} Mode;

typedef enum SubName {
    kFL     = 0,
    kFR     = 1,
    kRL     = 2,
    kRR     = 3
} SubName;

typedef struct SubModule {
    uint led_pin;
    uint cs_pin;
    float result;
    bool oor_flag;
} SubModule;

uint8_t line_vertical_position[NUM_SUBS] = { 4, 36, 68, 100 };

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

int pad_left_calc(float input);
void init_pins();
void init_hw();
void print_normal_numbers();
void print_cross_numbers();
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
                            fillRect(62, 40, 36, 48, ST7735_BLACK);
                            drawRectWH(62, 40, 36, 48, ST7735_WHITE);
                            drawText(65, 43, "%", ST7735_WHITE, ST7735_BLACK, 6);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            drawRectWH(62, 40, 36, 48, ST7735_BLACK);
                            drawText(65, 43, " ", ST7735_WHITE, ST7735_BLACK, 6);
                            drawFastHLine(10, 30, 100, ST7735_WHITE);
                            drawFastHLine(10, 62, 100, ST7735_WHITE);
                            drawFastHLine(10, 94, 100, ST7735_WHITE);
                            //move the mode indication bar to the correct position for next mode
                            drawFastVLine(0, 0, 43, ST7735_BLACK);
                            drawFastVLine(0, 44, 42, ST7735_WHITE);
                            mode_now = kPercent;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_KG();
                    }
                    break;

                case kPercent:
                    if (mode_next == kCross) {
                        mode_switch_cnt++;
                        if (mode_switch_cnt == 1) {
                            //draw rectangle to indicate switch to cross mode
                            fillRect(44, 40, 72, 48, ST7735_BLACK);
                            drawRectWH(44, 40, 72, 48, ST7735_WHITE);
                            drawText(47, 43, "cr", ST7735_WHITE, ST7735_BLACK, 6);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            drawRectWH(44, 40, 72, 48, ST7735_BLACK);
                            drawText(47, 43, "  ", ST7735_WHITE, ST7735_BLACK, 6);
                            drawFastHLine(10, 30, 100, ST7735_WHITE);
                            drawFastHLine(10, 62, 100, ST7735_WHITE);
                            drawFastHLine(10, 94, 100, ST7735_WHITE);
                            //move the mode indication bar to the correct position for next mode
                            drawFastVLine(0, 44, 42, ST7735_BLACK);
                            drawFastVLine(0, 85, 43, ST7735_WHITE);
                            print_cross_numbers();
                            mode_now = kCross;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_percent();
                    }
                    break;

                case kCross:
                    if (mode_next == kKilogram) {
                        mode_switch_cnt++;
                        if (mode_switch_cnt == 1) {
                            //draw rectangle to indicate switch to kg mode
                            fillRect(44, 40, 72, 48, ST7735_BLACK);
                            drawRectWH(44, 40, 72, 48, ST7735_WHITE);
                            drawText(47, 43, "kg", ST7735_WHITE, ST7735_BLACK, 6);
                        } else if (mode_switch_cnt > 10) {
                            //clear rectangle after time has elapsed
                            drawRectWH(44, 40, 72, 48, ST7735_BLACK);
                            drawText(47, 43, "  ", ST7735_WHITE, ST7735_BLACK, 6);
                            drawFastHLine(10, 30, 100, ST7735_WHITE);
                            drawFastHLine(10, 62, 100, ST7735_WHITE);
                            drawFastHLine(10, 94, 100, ST7735_WHITE);
                            //move the mode indication bar to the correct position for next mode
                            drawFastVLine(0, 85, 43, ST7735_BLACK);
                            drawFastVLine(0, 0, 43, ST7735_WHITE);
                            print_normal_numbers();
                            mode_now = kKilogram;
                            mode_switch_cnt = 0;
                        }
                    } else {
                        print_cross();
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

// can be used to pad number outputs e.g. sprintf(disp_buf, "2: %*.1f", pad_left_calc(result_sub1), result_sub1);
int pad_left_calc(float input) {
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

    spi_init(SPI_TFT_PORT, 10 * 1000 * 1000); // SPI with 10Mhz
    gpio_set_function(SPI_TFT_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TFT_TX, GPIO_FUNC_SPI);

    init_pins();

    //gpio_set_pulls(BTN_IN, 1, 0);

    btn_last = gpio_get(BTN_IN);
}

void print_normal_numbers(){
    sprintf(disp_buf, "1:");
    drawText(5, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "2:");
    drawText(5, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "3:");
    drawText(5, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "4:");
    drawText(5, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
}

void print_cross_numbers(){
    sprintf(disp_buf, "12");
    drawText(5, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "34");
    drawText(5, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "14");
    drawText(5, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "23");
    drawText(5, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
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

    print_normal_numbers();
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

void print_KG() {
    for (int i = 0; i < NUM_SUBS; i++) {
        if (sub_modules[i].oor_flag == true) {
            drawText(39, line_vertical_position[i], "   OOR", ST7735_WHITE, ST7735_BLACK, 3);
        } else {
            sprintf(disp_buf, "%*s%.1f", pad_left_calc(sub_modules[i].result), "", sub_modules[i].result);
            drawText(39, line_vertical_position[i], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
        }

    }
}

void print_percent() {
    float result_sum    = 0.0f;
    uint8_t oor_akk     = 0;

    for (int i = 0; i < NUM_SUBS; i++) {
        result_sum += sub_modules[i].result;
        oor_akk    += (uint8_t)sub_modules[i].oor_flag;
    }
    if ((result_sum == 0) || (oor_akk > 0)) {
        for (int i = 0; i < NUM_SUBS; i++) {
            drawText(39, line_vertical_position[i], "    NA", ST7735_WHITE, ST7735_BLACK, 3);
        }
    } else {
        for (int i = 0; i < NUM_SUBS; i++) {
            int result_sub_percent = (int)((sub_modules[i].result * 100.0) / result_sum);
            sprintf(disp_buf, "%*s%d", pad_left_calc(result_sub_percent) + 2, "", result_sub_percent);
            drawText(39, line_vertical_position[i], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
        }
    }
}

void print_cross() {
    float result_sum    = 0.0f;
    uint8_t oor_akk     = 0;

    for (int i = 0; i < NUM_SUBS; i++) {
        result_sum += sub_modules[i].result;
        oor_akk    += (uint8_t)sub_modules[i].oor_flag;
    }
    if ((result_sum == 0) || (oor_akk > 0)) {
        for (int i = 0; i < NUM_SUBS; i++) {
            drawText(39, line_vertical_position[i], "    NA", ST7735_WHITE, ST7735_BLACK, 3);
        }
    } else {
        //print front
        int result_front = (int)(((sub_modules[kFL].result + sub_modules[kFR].result) * 100.0) / result_sum);
        sprintf(disp_buf, "%*s%d", pad_left_calc(result_front) + 2, "", result_front);
        drawText(39, line_vertical_position[0], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);

        //print rear
        int result_rear = (int)(((sub_modules[kRL].result + sub_modules[kRR].result) * 100.0) / result_sum);
        sprintf(disp_buf, "%*s%d", pad_left_calc(result_rear) + 2, "", result_rear);
        drawText(39, line_vertical_position[1], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);

        //print cross FL+RR
        int result_cross_FLRR = (int)(((sub_modules[kFL].result + sub_modules[kRR].result) * 100.0) / result_sum);
        sprintf(disp_buf, "%*s%d", pad_left_calc(result_cross_FLRR) + 2, "", result_cross_FLRR);
        drawText(39, line_vertical_position[2], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);

        //print cross FR+RL
        int result_cross_FRRL = (int)(((sub_modules[kFR].result + sub_modules[kRL].result) * 100.0) / result_sum);
        sprintf(disp_buf, "%*s%d", pad_left_calc(result_cross_FRRL) + 2, "", result_cross_FRRL);
        drawText(39, line_vertical_position[3], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    }
}