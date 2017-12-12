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

#include "state_manager.h"
#include <string.h>

#define IRQTESTER_DRV_NAME "irqtester0"


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
	printk("Starting irqtester test on arch: %s\n", CONFIG_ARCH);

	// get driver handles
	struct device *dev;
	dev = device_get_binding(IRQTESTER_DRV_NAME);

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	
	test_uint_overflow();
		
	int i=0;
	bool abort = false;
	while(!abort){
		i++;
		printk("Entering cycle %i. Global max %i \n", i, global_max_cyc);

		
		// timing
		//run_test_timing_rx(dev);
		//run_test_min_timing_rx(dev);
		run_test_state_mng_1(dev);

	}
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

