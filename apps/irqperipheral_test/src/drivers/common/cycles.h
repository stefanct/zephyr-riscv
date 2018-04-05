/**
 * @file
 * @brief  Deal with riscv32 hardware cpu cycles.
 *
 * Extension to macros in sys_cloch.h for cpu cycle based units.
 * Mind the difference between sysclock (or rtc) cycles and cpu cycles! 
 */



#ifndef CYCLES_H
#define CYCLES_H

#include <zephyr.h>


u32_t get_cycle_32();
void cycle_busy_wait(u32_t t_cyc);


/**
 * @brief Convert cpu cycles to integer milliseconds >= 1.
 * 
 * Warning: Cpu cycles are less accurate than sysclock cycles!
 */
#define CYCLES_CYC_2_MS(t_cyc) (t_cyc <= 1000*CONFIG_APP_RISCV_CORE_FREQ_MHZ ? 1 : (t_cyc / (1000*CONFIG_APP_RISCV_CORE_FREQ_MHZ)))
#define CYCLES_CYC_2_US(t_cyc) (t_cyc <= CONFIG_APP_RISCV_CORE_FREQ_MHZ ? 1 : (t_cyc / (CONFIG_APP_RISCV_CORE_FREQ_MHZ)))
/**
 * @brief Convert milliseconds to cpu cycles >= 1.
 * 
 * Warning: Cpu cycles are less accurate than sysclock cycles!
 */
#define CYCLES_MS_2_CYC(t_ms) (1000*CONFIG_APP_RISCV_CORE_FREQ_MHZ * t_ms)
#define CYCLES_US_2_CYC(t_ms) (CONFIG_APP_RISCV_CORE_FREQ_MHZ * t_ms)

#endif