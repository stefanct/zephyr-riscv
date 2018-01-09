#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

#include <device.h>

void run_test_hw_basic_1(struct device * dev);
void run_test_timing_rx(struct device * dev);
void run_test_min_timing_rx(struct device * dev);
void run_test_sm1_throughput_1(struct device * dev);
void run_test_sm1_throughput_2(struct device * dev);
void run_test_irq_throughput_3_autoadj(struct device * dev);
void run_test_poll_throughput_1_autoadj(struct device * dev);

#endif