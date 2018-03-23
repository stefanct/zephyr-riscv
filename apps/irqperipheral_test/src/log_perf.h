
/**
 * @file 
 * @brief  Implements a performance logging method which prints
 *          to a fifo string buffer instead of uart.
 */

#ifndef LOG_PERF_H
#define LOG_PERF_H

#include <misc/printk.h>


// takes a lot space in data segment, so try to keep short
#define LOG_PERF_BUFFER_DEPTH CONFIG_APP_LOG_PERF_BUFFER_DEPTH      // 10
#define LOG_PERF_STRING_LEN CONFIG_APP_LOG_PERF_STRING_LEN          // 60


void print_to_buf(const char *fmt, ...);
void print_buff();
void save_to_intbuf(int id, int msg);

#if LOG_PERF_BUFFER_DEPTH == 0
#define LOG_PERF(...){;}
#else
#define LOG_PERF(...) \
    print_to_buf(__VA_ARGS__)
#endif

#if LOG_PERF_BUFFER_DEPTH == 0
#define LOG_PERF_INT(id, x){;}
#else
#define LOG_PERF_INT(id, x) \
    save_to_intbuf(id, x)
#endif

#define PRINT_LOG_BUFF() do{\
    print_buff(); \
}while(0)
#endif