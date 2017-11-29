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
	printk("Starting irqtester test on %s\n", CONFIG_ARCH);

	struct device *dev;
	dev = device_get_binding(IRQTESTER_DRV_NAME);
	dev_global = dev;

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	int NUM_RUNS = 100;

	test_queue_rx_timing(dev, NUM_RUNS);
	test_interrupt_timing(dev, NUM_RUNS);


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

}

