#include "log_perf.h"
#include "utils.h"
#include <misc/printk.h>


// takes a lot space in data segment, so try to keep short
#define LOG_PERF_BUFFER_DEPTH 30
#define LOG_PERF_STRING_LEN 60

typedef char logline[LOG_PERF_STRING_LEN];
logline log_buff[LOG_PERF_BUFFER_DEPTH];

static int i_log_call;

void print_to_buf(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    vsnprintk(log_buff[i_log_call % LOG_PERF_BUFFER_DEPTH], LOG_PERF_STRING_LEN, fmt, args); 
    i_log_call++; 

    va_end(args);
}

void print_buff(){
    int len = (i_log_call < LOG_PERF_BUFFER_DEPTH ? i_log_call : LOG_PERF_BUFFER_DEPTH); 
    //printk("i_log_calls %u \n", i_log_call);
    printk("Printing LOG_PERF buffer: \n");
    for(int i=0; i<len; i++){
        printk("%s\n", log_buff[i]);
    }
}