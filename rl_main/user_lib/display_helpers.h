// Author: Christoph Deussen
//
// Helper functions for project specific display

#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

#define MAX_PADDING     3
#define NUM_SUBS        4

typedef struct SubModule {
    uint led_pin;
    uint cs_pin;
    float result;
    bool oor_flag;
} SubModule;

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

int pad_left_calc(float input);
void print_normal_numbers(char disp_buf[]);
void print_cross_numbers(char disp_buf[]);
void draw_mode_indicator_text(Mode mode_next);
void clear_mode_indicator_text(Mode mode_next);
void set_mode_indicator_bar(Mode mode_next);
void print_KG(SubModule sub_modules[],char disp_buf[]);
void print_percent(SubModule sub_modules[],char disp_buf[]);
void print_cross(SubModule sub_modules[],char disp_buf[]);

#endif