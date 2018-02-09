/**
 * @file
 * @brief  Simple statistical profiler employing periodic IRQ
 *          of irqtestperipheral hardware.
 */

#ifndef PROFILER_H
#define PROFILER_H

#include "zephyr.h"

void profiler_enable(u32_t period_cyc);
void profiler_start();
void profiler_stop();

#endif