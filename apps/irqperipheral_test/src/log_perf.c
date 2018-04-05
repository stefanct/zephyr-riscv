#include "log_perf.h"
#include "utils.h"
#include <misc/printk.h>
#include <logging/sys_log.h>

#ifndef TEST_MINMAL


typedef char logline[LOG_PERF_STRING_LEN];
logline log_buff[LOG_PERF_BUFFER_DEPTH];

typedef int logline_int[2];
logline_int log_intbuff[LOG_PERF_BUFFER_DEPTH];

static int i_log_call;
static int i_logint_call;

void print_to_buf(const char *fmt, ...){

    #if(LOG_PERF_BUFFER_DEPTH == 0)
        return;
    #else
    

    va_list args;
    va_start(args, fmt);

    vsnprintk(log_buff[i_log_call % LOG_PERF_BUFFER_DEPTH], LOG_PERF_STRING_LEN, fmt, args); 
    i_log_call++; 

    va_end(args);
    #endif
}

void save_to_intbuf(int id, int msg){
    #if(LOG_PERF_BUFFER_DEPTH == 0)
    return;
    #else

    log_intbuff[i_logint_call % LOG_PERF_BUFFER_DEPTH][0] = id;
    log_intbuff[i_logint_call % LOG_PERF_BUFFER_DEPTH][1] = msg;
    i_logint_call++; 
    #endif
}

static void purge_buff(){

    for(int i=0; i<LOG_PERF_BUFFER_DEPTH; i++){
        log_intbuff[i][0] = 0;
        log_intbuff[i][1] = 0;
        vsnprintk(log_buff[i], LOG_PERF_STRING_LEN, "", NULL);
    }

    i_log_call = 0;
}

void print_buff(int verb){
    #if(LOG_PERF_BUFFER_DEPTH == 0)
        printkv(1, "WARNING: Printing zero-sized log buffer. Check Kconfig. \n");
        return;
    #endif

    int len = (i_log_call < LOG_PERF_BUFFER_DEPTH ? i_log_call : LOG_PERF_BUFFER_DEPTH); 
    int len_int = (i_logint_call < LOG_PERF_BUFFER_DEPTH ? i_logint_call : LOG_PERF_BUFFER_DEPTH);   

    //printk("i_log_calls %u \n", i_log_call);
    printkv(verb, "Printing LOG_PERF buffer: \n");
    for(int i=0; i<len; i++){
        printkv(verb, "%s\n", log_buff[i]);
    }
    printkv(verb, "Printing LOG_PERF int buffer: \n");
    for(int i=0; i<len_int; i++){
        printkv(verb, "[%i] %i\n", log_intbuff[i][0], log_intbuff[i][1]);
    } 

    purge_buff();
}

#endif // TEST_MINIMAL