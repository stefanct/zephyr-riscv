#include "cycles.h"
#include <stdio.h>
#include "encoding.h"


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
// debug only!
//#define rdmcycle(x)({*(x)=0;})




/// read lower 32 bits of cycle csr register
u32_t get_cycle_32(){
    u32_t cycle;
    return rdmcycle(&cycle);
}
    