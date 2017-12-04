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

#ifndef UTILS_C
#define UTILS_C

#include <zephyr.h>
#include "timestamp.h"


#ifdef CONFIG_PRINTK
#include <misc/printk.h>
#include <stdio.h>
extern char tmp_string[];

#define TMP_STRING_SIZE  100

#define PRINT(fmt, ...) printk(fmt, ##__VA_ARGS__)
#define PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define PRINT_FORMAT(fmt, ...)						\
	do {                                                            \
		snprintf(tmp_string, TMP_STRING_SIZE, fmt, ##__VA_ARGS__); \
		PRINTF("|%-77s|\n", tmp_string);				\
	} while (0)

/**
 *
 * @brief Print dash line
 *
 * @return N/A
 */
static void print_dash_line(void)
{
	PRINT("|-------------------------------------------------------"
	       "----------------------|\n");
}

void print_report(int err_count){
    if(err_count == 0){
        PRINT("All tests PASSED \n");
    }
    else{
        PRINT("Tests FAILED with %i errors \n", err_count);
    }
    print_dash_line();
}

#define PRINT_END_BANNER()						\
	do {								\
	PRINT("|                                    E N D             " \
	       "                       |\n");				\
	print_dash_line();						\
	} while (0)


#define PRINT_BANNER()						\
	do {								\
	print_dash_line();						\
	PRINT("|                            Latency Benchmark         " \
	       "                       |\n");				\
	print_dash_line();						\
	} while (0)


#define PRINT_TIME_BANNER()						\
	do {								\
	PRINT_FORMAT("  tcs = timer clock cycles: 1 tcs is %u nsec",	\
		     SYS_CLOCK_HW_CYCLES_TO_NS(1));			\
	print_dash_line();						\
	} while (0)

#define PRINT_OVERFLOW_ERROR()			\
	PRINT_FORMAT(" Error: tick occurred")



#else
#error PRINTK configuration option needs to be enabled
#endif


/* scratchpad for the string used to print on console */
char tmp_string[TMP_STRING_SIZE];



#endif