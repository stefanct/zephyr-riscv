/**
 * @file
 * @brief  Simple statistical profiler employing periodic IRQ
 *          of irqtestperipheral hardware.
 */

#ifndef PROFILER_H
#define PROFILER_H

#include "zephyr.h"

#define PROFILER_BUF_DEPTH CONFIG_APP_PROFILER_BUF_DEPTH    // 1000, reasonable if activated


void profiler_enable(u32_t period_cyc);
void profiler_start();
void profiler_stop();

#endif