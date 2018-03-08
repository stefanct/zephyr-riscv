/**
 * @file
 * @brief Run different bundled tests and do nice printing
 */

#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

#include <device.h>

int run_test_hw_basic_1(struct device * dev);
void run_test_timing_rx(struct device * dev);
void run_test_min_timing_rx(struct device * dev);

void run_test_state_mng_1(struct device * dev);

void run_test_sm1_throughput_1(struct device * dev);
void run_test_sm_throughput_2(struct device * dev, int id_sm);
void run_test_sm2_action_perf_3(struct device * dev);

int run_test_irq_throughput_3_autoadj(struct device * dev);
void run_test_poll_throughput_1_autoadj(struct device * dev);

#endif