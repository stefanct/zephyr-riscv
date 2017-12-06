#ifndef TESTS_H
#define TESTS_H

#include <device.h>

// ugly globals
extern int error_count;
extern int error_stamp; // save between single tests


void test_interrupt_timing(struct device * dev, int timing_res[], int num_runs, int verbose);
void test_rx_timing(struct device * dev, int timing_res[], int num_runs, int mode, int verbose);


void print_analyze_timing(int timing[], int len, int verbosity);

void irq_handler_mes_time(void);

#endif