#ifndef LOG_PERF_H
#define LOG_PERF_H

#include <misc/printk.h>

/**
 * @file    Implements a performance logging method which prints
 *          to a fifo string buffer instead of uart.
 * 
 */




void print_to_buf(const char *fmt, ...);
void print_buff();


#define LOG_PERF(...) \
    print_to_buf(__VA_ARGS__)
    

#define PRINT_LOG_BUFF() do{\
    print_buff(); \
}while(0)
#endif