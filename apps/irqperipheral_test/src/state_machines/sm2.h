/**
 * @file
 * @brief   Implementation of State Machine SM2.
 *          For testing and benchmarkin with workload (actions).
 */


#ifndef SM2_H
#define SM2_H

#include <device.h>

void sm2_config(int users, int usr_per_batch, void (*ul_task)(void), int param, int pos_param);
void sm2_init(struct device * dev, int period_irq1_us, int period_irq2_us);
void sm2_run();
void sm2_fire_irqs(int period_irq1_us, int period_irq2_us);

void sm2_print_report();
void sm2_reset();


#endif