#ifndef CYCLES_H
#define CYCLES_H

#include <zephyr.h>

u32_t get_cycle_32();
void cycle_busy_wait(u32_t t_cyc);

#endif