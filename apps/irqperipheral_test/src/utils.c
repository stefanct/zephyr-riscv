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

#ifndef TEST_MINIMAL

#ifdef CONFIG_PRINTK
#include <misc/printk.h>
#include <stdio.h>
#include <string.h>
#else
#warning PRINTK configuration option needs to be enabled
#endif


static int verbosity_thresh = -1;


void print_set_verbosity(int verb){
    printk("DEBUG: Set verb to %i \n", verb);
    verbosity_thresh = verb;
}

int print_get_verbosity(){
    return verbosity_thresh;
}

static bool do_print(int verbosity, bool exact){
    bool cond1 = (verbosity_thresh >= verbosity);
    if(exact)
        cond1 = (verbosity_thresh == verbosity);

    return (cond1 || verbosity == 0 || verbosity_thresh < 0);
}

/**
 * @brief: printk if given verbosity >= set verbosity threshold. 
 * 
 * @param verbosity: print always if 0
 */

void printkv(int verbosity, const char *fmt, ...){

    // predict branch assuming verbosity is not high
    if(unlikely(do_print(verbosity, false))){
        va_list args;
        va_start(args, fmt);

        vprintk(fmt, args); 
        va_end(args);
    }
}
/**
 * @brief: printk if given verbosity exactly == set verbosity threshold. 
 * 
 * @param verbosity: print always if 0
 */

void printkve(int verbosity, const char *fmt, ...){

    if(unlikely(do_print(verbosity, true))){
        va_list args;
        va_start(args, fmt);

        vprintk(fmt, args); 
        va_end(args);
    }
}


/**
 * @brief Print dash line
 *
 * @return N/A
 */
void print_dash_line(int verbosity){
	printkv(verbosity, "|-------------------------------------------------------"
	       "----------------------|\n");
}

void print_report(int verbosity, int err_count){
	print_dash_line(verbosity);
    if(err_count == 0){
        printk_framed(verbosity, "All tests PASSED");
    }
    else{
        printk_framed(verbosity, "Tests FAILED with %i errors", err_count);
    }
    print_dash_line(verbosity);

}

void print_end_banner(int verbosity)	{								
	printkv(verbosity, "|                                    E N D             " \
	       "                       |\n");				\
	print_dash_line(verbosity);						\
} 


void print_banner(int verbosity)	 {								
	print_dash_line(verbosity);						\
	printkv(verbosity, "|                            Latency Benchmark         " \
	       "                       |\n");				\
	print_dash_line(verbosity);						\
} 


void print_time_banner(int verbosity){								
	printk_framed(verbosity, "tcs = timer clock cycles: 1 tcs is %u nsec",	
		     SYS_CLOCK_HW_CYCLES_TO_NS(1));			
	print_dash_line(verbosity);						
} 



/**
 * @brief prints string in the center of console
 * 
 * Automatically adds \n identifier for end of line.
 * Uses standard width value of 80 for console.
 */
void printk_centered(int verbosity, const char *fmt, ...){
    va_list args;
	va_start(args, fmt);

    if(!do_print(verbosity, false))
        return;

	

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
void printk_framed(int verbosity, const char *fmt, ...){

    va_list args;
	va_start(args, fmt);

    if(!do_print(verbosity, false))
        return;

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


void print_arr_int(int verbosity, int arr[], int len){
    int i;
    for (i=0; i < len; i++) {
        if(i == len-1)
            printkv(verbosity, "%i", (int)arr[i]);
        else
            printkv(verbosity, "%i, ", (int)arr[i]);
    }
    printkv(verbosity, "\n");
}

void snprint_arr_int(char * str, int len_str, int arr[], int len){
    // credits
    // https://stackoverflow.com/questions/30234363/how-can-i-store-an-int-array-into-string
    int i;
    int i_str = 0;
    int add_i_str = 0;

    for (i=0; i < len; i++) {
        add_i_str = 0;
        if(i == len-1)
            add_i_str = snprintk(str+i_str, len_str-i_str, "%i",  (int)arr[i]);
        else
            add_i_str = snprintk(str+i_str, len_str-i_str, "%i, ", (int)arr[i]);
        // buffer full
        if(add_i_str < 0)
            break;
        i_str += add_i_str;      
    }
  
}

void snprint_arr_p(char * str, int len_str, void * arr[], int len){
    // credits
    // https://stackoverflow.com/questions/30234363/how-can-i-store-an-int-array-into-string
    int i;
    int i_str = 0;
    int add_i_str = 0;

    for (i=0; i < len; i++) {
        add_i_str = 0;
        if(i == len-1)
            add_i_str = snprintk(str+i_str, len_str-i_str, "%p",  (void*)arr[i]);
        else
            add_i_str = snprintk(str+i_str, len_str-i_str, "%p, ", (void*)arr[i]);
        // buffer full
        if(add_i_str < 0)
            break;
        i_str += add_i_str;
        
    }
}

void print_arr_uint(int verbosity, u32_t arr[], int len){
    int i;
    for (i=0; i < len; i++) {
        if(i == len-1)
            printkv(verbosity, "%u", (int)arr[i]);
        else
            printkv(verbosity, "%u, ", (int)arr[i]);
    }
    printkv(verbosity, "\n");
}


void dbg_read_mem(u32_t * p_mem){
	u32_t reg;
	// write pointer p_mem to a5
	__asm__ volatile("mv a5, %0" :: "r" (p_mem));
	// load mem at p_mem to a4
	__asm__ volatile("lw a4, 0(a5)");
	// read a4
	__asm__ volatile("addi %0, a4, 0" : "=r" (reg));
	
	return reg;

}


#endif // TEST_MINIMAL