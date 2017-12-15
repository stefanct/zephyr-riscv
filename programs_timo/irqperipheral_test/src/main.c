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

#include "state_manager.h"
#ifndef TEST_MINIMAL
#include <string.h>
#endif

#define IRQTESTER_DRV_NAME "irqtester0"
#define IRQTESTER_HW_REV 1


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



void main(void)
{	
	print_device_drivers();
	printk("Starting irqtester test for hw rev. %i on arch: %s\n", IRQTESTER_HW_REV, CONFIG_ARCH);

	// get driver handles
	struct device *dev;
	dev = device_get_binding(IRQTESTER_DRV_NAME);

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	
	//test_uint_overflow();
	//run_test_hw_basic_1(dev);

	int i=0;
	bool abort = false;
	do{
		i++;
		printk("Entering cycle %i. Global max %i \n", i, global_max_cyc);

		
		// timing
		//run_test_timing_rx(dev);
		run_test_min_timing_rx(dev);
		//run_test_state_mng_1(dev);

	}while(!abort);
	

	printk("Total uptime %u ms \n", k_uptime_get_32());// not working
	printk("***** I'm done here... Goodbye! *****");
}

