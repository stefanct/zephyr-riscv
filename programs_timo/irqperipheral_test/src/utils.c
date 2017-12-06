/* utils.c - modified utility functions, originially used by tests/benchmark/latency measurement */

/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DESCRIPTION
 * this file contains commonly used functions
 */


#include <zephyr.h>
#include "utils.h"

#ifdef CONFIG_PRINTK
#include <misc/printk.h>
#include <stdio.h>
#else
#error PRINTK configuration option needs to be enabled
#endif


/**
 *
 * @brief Print dash line
 *
 * @return N/A
 */
void print_dash_line(void){
	printk("|-------------------------------------------------------"
	       "----------------------|\n");
}

void print_report(int err_count){
	print_dash_line();
    if(err_count == 0){
        printk("All tests PASSED \n");
    }
    else{
        printk("Tests FAILED with %i errors \n", err_count);
    }
    print_dash_line();
}

void print_end_banner()	{								
	printk("|                                    E N D             " \
	       "                       |\n");				\
	print_dash_line();						\
} 


void print_banner()	 {								
	print_dash_line();						\
	printk("|                            Latency Benchmark         " \
	       "                       |\n");				\
	print_dash_line();						\
} 


void print_time_banner(){								
	printk("  tcs = timer clock cycles: 1 tcs is %u nsec",	
		     SYS_CLOCK_HW_CYCLES_TO_NS(1));			
	print_dash_line();						
} 




