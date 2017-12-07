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
        printk_framed("All tests PASSED \n");
    }
    else{
        printk_framed("Tests FAILED with %i errors \n", err_count);
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
	printk_framed("tcs = timer clock cycles: 1 tcs is %u nsec",	
		     SYS_CLOCK_HW_CYCLES_TO_NS(1));			
	print_dash_line();						
} 

/**
 * @brief prints string in the center of console
 * 
 * Only works if printf is avilable.
 * 
 * @param width: of console, suggested standard value is 80
 */
void print_centered(const char *string, int width){

	// do nothing if string doesn't fit in a single row
	if(strlen(string) >= width)
		printf("%s", string);
	else
		printf("%*s", (int)(width/2 + strlen(string)/2), string);
}
 
/**
 * @brief 	prints string in the center of console with | at beginning
 * 			and end of line. 
 * 
 * Only works if printf is avilable.
 * 
 */
void print_framed(const char *string){
    int width = 80;
	printf("|");
	print_centered(string, width - 2);
	printf("%*s",(int)((width-2)/2-(strlen(string))/2),"|");
	printf("\n");
}

/**
 * @brief prints string in the center of console
 * 
 * Automatically adds \n identifier for end of line.
 * Uses standard width value of 80 for console.
 * Todo: make variadic function to support passing values to print
 */
void printk_centered(const char *fmt, ...){

	va_list args;
	va_start(args, fmt);

    size_t str_length = vsnprintk(NULL, 0, fmt, args);
	int width = 80-1;

    // if a new length is less or equal length of the original string, returns the original string
    if (width <= str_length){
        vprintk(fmt, args);
		return;
	}

    unsigned int i, total_rest_length;

    // length of a wrapper of the original string
    total_rest_length = width - str_length;

    // write a prefix
    i = 0;
    while (i < (total_rest_length / 2)) {
        printk(" ");
        ++i;
    }
 
	vprintk(fmt, args);
	  
    va_end(args);

}

/**
 * @brief 	prints string in the center of console with | at beginning
 * 			and end of line. 
 * 
 * Automatically adds \n identifier for end of line.
 * Uses standard width value of 80 for console.
 */
void printk_framed(const char *fmt, ...){
    
	va_list args;
	va_start(args, fmt);
	
	size_t str_length = vsnprintk(NULL, 0, fmt, args);
	//printk("DEBUG: Length of string %i \n", str_length);
	int width = 80-1;
	width -= 2; // for framing

    // if a new length is less or equal length of the original string, returns the original string
    if (width <= str_length){
        vprintk(fmt, args);
		return;
	}

    unsigned int i, total_rest_length;

    // length of a wrapper of the original string
    total_rest_length = width - str_length;

    // write a prefix 
    i = 0;
	printk("|");
    while (i < (total_rest_length / 2)) {
        printk(" ");
        ++i;
    }
 
    // write the original string
    //printk("%s", string);
    vprintk(fmt, args);
	  
    va_end(args);

    // write a postfix 
    i += str_length;
    while (i < width) {
        printk(" ");
        ++i;
    }
	printk("|\n");
}