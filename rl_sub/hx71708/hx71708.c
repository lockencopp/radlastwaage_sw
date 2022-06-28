#include "hardware/gpio.h"
#include "pico/time.h"
#include "hx71708.h"

inline void delay_asm(int clocks) {
    for (int i = 0; i < clocks; i++) {
        __asm__("NOP");
    }
}

void HX71708_reset() {
    gpio_put(HX1_SCK, 1);
    gpio_put(HX2_SCK, 1);
    sleep_us(200);
    gpio_put(HX1_SCK, 0);
    gpio_put(HX2_SCK, 0);
    sleep_ms(400);
}

void HX71708_init() {
    gpio_init(HX1_DOUT);
    gpio_init(HX1_SCK);
    gpio_init(HX2_DOUT);
    gpio_init(HX2_SCK);
    gpio_set_dir(HX1_DOUT, GPIO_IN);
    gpio_set_dir(HX2_DOUT, GPIO_IN);
    gpio_set_dir(HX1_SCK, GPIO_OUT);
    gpio_set_dir(HX2_SCK, GPIO_OUT);

    HX71708_reset();
}

int HX71708_read(HX71708_t *inst) {
    int hx_data = 0;
    int j = 23;
    for (int i = 0; i < 25; i++) {
        gpio_put(inst->sck, 1);

        if (j >= 0)
            hx_data |= (gpio_get(inst->dout) << j);
        else
            delay_asm(2);

        j--;

        gpio_put(inst->sck, 0);
        delay_asm(3);
    }
    if (hx_data > 0x7fffff) {
        hx_data -= 0x1000000;
    }
    if ((inst->offset_counter < OFFSET_NUM)) {
        inst->offset += hx_data;
        inst->offset_counter++;
        return 0;
    } else if (inst->offset_counter == OFFSET_NUM) {
        inst->offset = inst->offset / OFFSET_NUM;
        inst->offset_counter++;
    }

    inst->history[inst->history_index] = hx_data;
    inst->history_index++;
    if (inst->history_index > (HIST_NUM - 1)) inst->history_index = 0;

    int sum = 0;
    for (int i = 0; i < HIST_NUM; i++) {
        sum += inst->history[i];
    }
    sum = sum / HIST_NUM;

    inst->output = sum - inst->offset;

    inst->sample_stats.sample_now = time_us_64() / 1000;
    inst->sample_stats.sample_time = (inst->sample_stats.sample_now - inst->sample_stats.sample_last);
    inst->sample_stats.sample_last = inst->sample_stats.sample_now;

    return inst->output;
}