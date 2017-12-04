#ifndef TESTS_H
#define TESTS_H

void test_interrupt_timing(struct device * dev, int timing_res[], int num_runs, int verbose);
void test_rx_timing(struct device * dev, int timing_res[], int num_runs, int mode, int verbose);

void print_analyze_timing(int timing[], int len);

#endif