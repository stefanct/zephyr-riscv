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

#ifndef UTILS_H
#define UTILS_H

#include <zephyr.h>

void print_dash_line(void);
void print_report(int err_count);
void print_banner(void);				
void print_end_banner(void);						
void print_time_banner(void);	

void printk_centered(const char *string, ...);
void printk_framed(const char *string, ...);

void print_arr_uint(u32_t arr[], int len);
void snprint_arr_int(char * str, int len_str, int arr[], int len);
void print_arr_int(int arr[], int len);

void snprint_arr_p(char * str, int len_str, void * arr[], int len);


#endif	


