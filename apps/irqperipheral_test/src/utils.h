/**
 * @file 
 * @brief  Useful print functions.
 * 
 * Partly based on tests/benchmark/latency_measurement
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */



#ifndef UTILS_H
#define UTILS_H

#include <zephyr.h>

void printkv(int verbosity, const char *fmt, ...);
void printkve(int verbosity, const char *fmt, ...);
void print_set_verbosity(int verb);
int print_get_verbosity();

void print_dash_line(int verbosity);
void print_report(int verbosity, int err_count);
void print_banner(int verbosity);				
void print_end_banner(int verbosity);						
void print_time_banner(int verbosity);	

void printk_centered(int verbosity, const char *string, ...);
void printk_framed(int verbosity, const char *string, ...);

void print_arr_uint(int verbosity, u32_t arr[], int len);
void print_arr_int(int verbosity, int arr[], int len);

void snprint_arr_int(char * str, int len_str, int arr[], int len);
void snprint_arr_p(char * str, int len_str, void * arr[], int len);


#endif	


