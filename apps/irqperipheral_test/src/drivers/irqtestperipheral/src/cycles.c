#include "cycles.h"
#include <stdio.h>
#include "../../common/encoding.h"

#define CYCLES_CPUCYC_PER_US 65 // todo: derive from CONFIG_ macro

// from sifive-freedom-sdk
// The mcycle counter is 64-bit counter, but since
// Freedom E platforms use RV32, we must access it as
// 2 32-bit registers. At 256MHz, the lower bits will
// roll over approx. every 5 seconds, so we check for
// rollover with this routine as suggested by the
// RISC-V Priviledged Architecture Specification.

#define rdmcycle(x)({				       \
    uint32_t lo, hi, hi2;			       \
    __asm__ __volatile__ ("1:\n\t"		       \
			  "csrr %0, mcycleh\n\t"       \
			  "csrr %1, mcycle\n\t"	       \
			  "csrr %2, mcycleh\n\t"       \
			  "bne  %0, %2, 1b\n\t"			\
			  : "=r" (hi), "=r" (lo), "=r" (hi2)) ;	\
    *(x) = lo | ((uint64_t) hi << 32); 				\
})


/// read lower 32 bits of cycle csr register
u32_t get_cycle_32(){
    u32_t cycle;
    return rdmcycle(&cycle);
}
    

/**
 * @brief Wait for at least t_cyc cpu cycles.
 * 
 * No upper limit of waiting time guaranteed!
 * Busy waiting currently, since no hw timers implemented yet.
 */ 
void cycle_busy_wait(u32_t t_cyc){
    
    u32_t start = get_cycle_32();
    u32_t now = start;

     while(t_cyc > (now - start)){
        now = get_cycle_32();
    }

}

u32_t cycle_us_2_cyc(u32_t t_us){
    return CYCLES_CPUCYC_PER_US * t_us;
}

/// Attention: result is rounded
u32_t cycle_cyc_2_us(u32_t t_cyc){
    return t_cyc / CYCLES_CPUCYC_PER_US;
}