#include <stdio.h>
#include "ST7735_TFT.h"
#include "display_helpers.h"

uint8_t line_vertical_position[NUM_SUBS] = { 4, 36, 68, 100 };

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

void print_normal_numbers(char disp_buf[]) {
    sprintf(disp_buf, "1:");
    drawText(5, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "2:");
    drawText(5, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "3:");
    drawText(5, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "4:");
    drawText(5, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
}

void print_cross_numbers(char disp_buf[]) {
    sprintf(disp_buf, "12");
    drawText(5, 4, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "34");
    drawText(5, 36, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "14");
    drawText(5, 68, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
    sprintf(disp_buf, "23");
    drawText(5, 100, disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
}

void draw_mode_indicator_text(Mode mode_next) {
    switch (mode_next) {
    case kPercent:
        fillRect(62, 40, 36, 48, ST7735_BLACK);
        drawRectWH(62, 40, 36, 48, ST7735_WHITE);
        drawText(65, 43, "%", ST7735_WHITE, ST7735_BLACK, 6);
        break;
    case kCross:
        fillRect(44, 40, 72, 48, ST7735_BLACK);
        drawRectWH(44, 40, 72, 48, ST7735_WHITE);
        drawText(47, 43, "cr", ST7735_WHITE, ST7735_BLACK, 6);
        break;
    case kKilogram:
        fillRect(44, 40, 72, 48, ST7735_BLACK);
        drawRectWH(44, 40, 72, 48, ST7735_WHITE);
        drawText(47, 43, "kg", ST7735_WHITE, ST7735_BLACK, 6);
        break;
    default:
        break;
    }
}

void clear_mode_indicator_text(Mode mode_next) {
    switch (mode_next) {
    case kPercent:
        drawRectWH(62, 40, 36, 48, ST7735_BLACK);
        drawText(65, 43, " ", ST7735_WHITE, ST7735_BLACK, 6);

        break;

    case kCross:
        drawRectWH(44, 40, 72, 48, ST7735_BLACK);
        drawText(47, 43, "  ", ST7735_WHITE, ST7735_BLACK, 6);

        break;

    case kKilogram:
        drawRectWH(44, 40, 72, 48, ST7735_BLACK);
        drawText(47, 43, "  ", ST7735_WHITE, ST7735_BLACK, 6);

        break;

    default:
        break;
    }
    drawFastHLine(10, 30, 100, ST7735_WHITE);
    drawFastHLine(10, 62, 100, ST7735_WHITE);
    drawFastHLine(10, 94, 100, ST7735_WHITE);
}

void set_mode_indicator_bar(Mode mode_next) {
    switch (mode_next) {
    case kPercent:
        drawFastVLine(0, 0, 43, ST7735_BLACK);
        drawFastVLine(0, 44, 42, ST7735_WHITE);
        break;

    case kCross:
        drawFastVLine(0, 44, 42, ST7735_BLACK);
        drawFastVLine(0, 85, 43, ST7735_WHITE);
        break;

    case kKilogram:
        drawFastVLine(0, 85, 43, ST7735_BLACK);
        drawFastVLine(0, 0, 43, ST7735_WHITE);
        break;

    default:
        break;
    }
}

void print_KG(SubModule sub_modules[], char disp_buf[]) {
    for (int i = 0; i < NUM_SUBS; i++) {
        if (sub_modules[i].oor_flag == true) {
            drawText(39, line_vertical_position[i], "   OOR", ST7735_WHITE, ST7735_BLACK, 3);
        } else {
            sprintf(disp_buf, "%*s%.1f", pad_left_calc(sub_modules[i].result), "", sub_modules[i].result);
            drawText(39, line_vertical_position[i], disp_buf, ST7735_WHITE, ST7735_BLACK, 3);
        }

    }
}

void print_percent(SubModule sub_modules[], char disp_buf[]) {
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

void print_cross(SubModule sub_modules[], char disp_buf[]) {
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