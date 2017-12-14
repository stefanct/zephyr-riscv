/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

void main(void)
{
	printk("Hello World! %s\n", CONFIG_ARCH);
	// test broken functions
	printk("Test of function %i \n", _ms_to_ticks(1));
	// works
	k_sleep(1);
	// works after fixing to NS32 
	//printk("Test of function %i \n", SYS_CLOCK_HW_CYCLES_TO_NS(10000)); // fixed
	
	
	printk("***** I'm done here... Goodbye! *****");
}
