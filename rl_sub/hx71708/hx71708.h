// Author: Christoph Deussen
//
// Driver for HX71708 load cell ADC

#ifndef HX71708_H
#define HX71708_H

#define OFFSET_NUM  3
#define HIST_NUM    3
#define HX1_DOUT    0
#define HX1_SCK     1
#define HX2_DOUT    2
#define HX2_SCK     3

typedef struct {
    uint sample_now;
    uint sample_last;
    uint sample_time;
} SampleStats_t;

typedef struct {
    uint dout;
    uint sck;
    int output;
    int offset;
    int offset_counter;
    int history[HIST_NUM];
    int history_index;
    SampleStats_t sample_stats;
} HX71708_t;

void HX71708_reset();
void HX71708_init();
int HX71708_read(HX71708_t *inst);

#endif