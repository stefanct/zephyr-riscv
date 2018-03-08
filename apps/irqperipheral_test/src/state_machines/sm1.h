#ifndef SM1_H
#define SM1_H

#include <device.h>

void sm1_run(struct device * dev, int period_irq1_us, int period_irq2_us);
void sm1_print_report();
void sm1_print_state_config();
void sm1_reset();
void sm1_print_cycles();

#endif