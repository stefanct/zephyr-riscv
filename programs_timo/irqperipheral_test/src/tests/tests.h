#ifndef TESTS_H
#define TESTS_H

#include <device.h>

// ugly globals
extern int error_count;
extern int error_stamp; // save between single tests

void test_uint_overflow();
void test_hw_rev_1_basic_1(struct device * dev);
void test_hw_rev_2_basic_1(struct device * dev);
void test_hw_rev_3_basic_1(struct device * dev);

void test_interrupt_timing(struct device * dev, int timing_res[], int num_runs, int verbose);
void test_rx_timing(struct device * dev, int timing_res[], int num_runs, int mode, int verbose);
void test_irq_throughput_1(struct device * dev, int delta_cyc, int * status_res, int num_runs);
void test_irq_throughput_2(struct device * dev, int period_cyc, int num_runs, int status_arr[], int len);


void test_state_mng_1(struct device * dev);


void print_analyze_timing(int timing[], int len, int verbosity);

void irq_handler_mes_time(void);



#endif