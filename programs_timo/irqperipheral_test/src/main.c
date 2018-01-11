/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>

#include "irqtestperipheral.h"
#include "test_runners.h"
#include "tests/tests.h"
#include "cycles.h"

#include "state_manager.h"
#ifndef TEST_MINIMAL
#include <string.h>
#endif
#include "state_machines/sm1.h"

#define IRQTESTER_DRV_NAME "irqtester0"
#define IRQTESTER_HW_REV 3


int global_max_cyc;

// thread data
//char __noinit __stack my_stack_area[1024];
//struct k_thread my_thread_data;


// copy of disabled function in devices.c
extern struct device __device_init_end[];
extern struct device __device_init_start[];
void device_list_get(struct device **device_list, int *device_count)
{

	*device_list = __device_init_start;
	*device_count = __device_init_end - __device_init_start;
}

// attention: uses "heavy" strcmp
void print_device_drivers(){
	#ifndef TEST_MINIMAL
	struct device *dev_list_buf;
	int len;

	device_list_get(&dev_list_buf, &len);

	printk("Found %i device drivers: \n", len);
	for(int i=0; i<len; i++){
		char * name = dev_list_buf[i].config->name;
		printk("%s @ %p, ", (strcmp("", name) == 0 ? "<unnamed>" : name), dev_list_buf[i].config->config_info);
	}
	printk("\n");
	#endif
}

// make static to keep handle after main thread is done
static struct device *dev;
// needed threads

void main(void)
{	
	printk("Current rtc / hw cycles: %u / %u \n", k_cycle_get_32(), get_cycle_32());
	print_device_drivers();
	printk("Starting irqtester test for hw rev. %i on arch: %s\n", IRQTESTER_HW_REV, CONFIG_ARCH);

	// get driver handles
	dev = device_get_binding(IRQTESTER_DRV_NAME);

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	
	//test_uint_overflow();
	run_test_hw_basic_1(dev);

	//run_test_irq_throughput_1(dev);
	//run_test_irq_throughput_2(dev);
	//run_test_irq_throughput_3_autoadj(dev);

	//run_test_poll_throughput_1_autoadj(dev);

	//run_test_sm1_throughput_1(dev);
	run_test_sm1_throughput_2(dev);
	

	int i=0;
	bool abort = true;
	do{
		i++;
		printk("Entering cycle %i. Global max %i \n", i, global_max_cyc);

		
		// timing
		//run_test_timing_rx(dev);
		//run_test_min_timing_rx(dev);
		//run_test_state_mng_1(dev);

	}while(!abort);
	

	printk("Total uptime / init time  %u ms \n", k_uptime_get_32());
	printk("***** Main thread: I'm done here... Goodbye! ***** \n");
	while(1){
		k_yield();
		// for some reason crashes when exiting main thread, must define threads in main?
	}
	return; // end main thread
}

