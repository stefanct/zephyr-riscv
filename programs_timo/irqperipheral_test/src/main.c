/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>

#include "irqtestperipheral.h"

#include "tests.c"
#include "utils.c"

#define IRQTESTER_DRV_NAME "irqtester0"

struct device *dev_global;

void isr_on_irq(void){
	int status = -1; 
	if(dev_global != NULL){
		irqtester_fe310_get_status(dev_global, &status);
		printk("Msg function called, device status %i \n", status);
	}
	else
		printk("Msg function called without dev configured \n");
}

void main(void)
{
	printk("Starting irqtester test on arch: %s\n", CONFIG_ARCH);
	// todo: 
	// - nice prints (see tests/benchmark/latency)
	// - error count


	struct device *dev;
	dev = device_get_binding(IRQTESTER_DRV_NAME);
	dev_global = dev;

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	#define NUM_RUNS 20
	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
	

	PRINT_BANNER();
	PRINT_TIME_BANNER();

	printk("Now running raw irq to isr timing test \n");
	irqtester_fe310_register_callback(dev, &irq_handler_mes_time);
	test_interrupt_timing(dev, timing_detailed_cyc, NUM_RUNS, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();

	printk("Now running timing test with queues and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with queues and generic reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_2);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();
		
	printk("Now running timing test with fifos and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with fifos and generic reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_2);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();

	printk("Now running timing test with semArr and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with semArr and generic reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_2);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();

	printk("Now running timing test with valflags and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with valflags and generic reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_2);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();

	// tests don't load values, so don't count errors here
	// -> reset error counter
	int errors_sofar = error_stamp;
	printk("INFO: Ignore the following error warnings for tests without load \n");
	printk("Now running timing test with noload, queue and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_3);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with noload, fifos and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_3);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with noload, semArr and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_3);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	printk("Now running timing test with noload, valflags and direct reg getters \n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_3);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();

	printk("Now running timing test with minimal, valflags\n");
	irqtester_fe310_register_callback(dev, _irq_0_handler_4);
	test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
	print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
	print_dash_line();
	// reset counter
	error_count = errors_sofar;

	print_report(error_count); // global var from tests.c

	/* test generic setters 
	struct DrvValue_uint val = {.payload=7}; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);
	struct DrvValue_int test;
	// firing loads register into driver values
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_perval: %i \n", test.payload);


	val.payload=42; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);
	// firing loads register into driver values
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_perval: %i \n", test.payload);

	// disabling won't fire the interrupt, so nothing loaded
	struct DrvValue_bool enable = {.payload=false}; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_perval: %i \n", test.payload);

	*/

	/* working _values array for data passing
	printk("Testing getter. Set perval: 42 \n");
	irqtester_fe310_enable(dev);
	irqtester_fe310_set_value(dev, 42);


	printk("Without firing irq, expect 0 from getter \n");
	struct DrvValue_int test;
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_perval: %i \n", test.payload);

	printk("Firing irq to load data, expect 42 from getter \n");
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_perval: %i \n", test.payload);

	printk("Trying to get invalid value. Please don't crash.. \n");
	irqtester_fe310_get_val(3, &test);
	printk("get_perval: %i \n", test.payload);
	*/


	/* first working example
	struct device *dev;
	dev = device_get_binding(IRQTESTER_DRV_NAME);
	dev_global = dev;

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	printk("irqtester device found, trying to register irq handler \n");
	irqtester_fe310_register_callback(dev, msg_on_irq);


	printk("trying to write to value mem. Expect value/perval: 42/0 \n");
	irqtester_fe310_set_value(dev, 42);
	unsigned int test;
	irqtester_fe310_get_perval(dev, &test);
	printk("perval %i \n", test);
	irqtester_fe310_get_value(dev, &test);
	printk("value %i \n", test);

	printk("enabling outputs. perval Expect 42 \n");
	irqtester_fe310_enable(dev);
	irqtester_fe310_set_value(dev, 42);
	irqtester_fe310_get_perval(dev, &test);
	printk("perval %i \n", test);

	printk("trying to fire interrupt... \n");
	test = irqtester_fe310_fire(dev);
	int status = -1;
	irqtester_fe310_get_status(dev_global, &status);
	printk("returned %i, device status %i \n", test, status);
	*/

	printk("Total uptime %u ms \n", k_uptime_get_32());// not working
	printk("***** I'm done here... Goodbye! *****");
}

