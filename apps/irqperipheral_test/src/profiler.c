#include "profiler.h"
#include "irqtestperipheral.h"

#ifndef TEST_MINIMAL

#define PROFILER_BUF_DEPTH 1000


static u32_t log_pc[PROFILER_BUF_DEPTH];



static void dump_2_console(){
    printk("{[");
    for(int i=0; i<PROFILER_BUF_DEPTH; i++){
        printk("%p,\n", (void *)log_pc[i]);
    }
    printk("]}\n");
}


static u32_t i_call_isr;
static void irq_handler_profiler_1(){
    
    // get program counter when interrupt detected
    u32_t pc;
	__asm__ volatile("csrr %0, mepc" : "=r" (pc));

    log_pc[i_call_isr] = pc;
    i_call_isr++;

    if(i_call_isr >= PROFILER_BUF_DEPTH){
        i_call_isr = 0;
        // deactivate irq2 interrupt to avoid error code
        // in status register
        struct DrvValue_uint reg_num = {.payload=0};
        struct DrvValue_uint reg_period = {.payload=0};	
        irqtester_fe310_get_reg(g_dev_cp, VAL_IRQ_2_PERIOD, &reg_period);
        irqtester_fe310_get_reg(g_dev_cp, VAL_IRQ_2_NUM_REP, &reg_num);
        u32_t save_num = reg_num.payload;
        u32_t save_period = reg_period.payload;
        
        dump_2_console();

        // activate again
        reg_period.payload = save_period;
        reg_num.payload = save_num;
        irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_PERIOD, &reg_period);
        irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_NUM_REP, &reg_num);
    }

    irqtester_fe310_clear_2(g_dev_cp);
    // fire in case of deactived, else no effect
    irqtester_fe310_fire_2(g_dev_cp);
}

/**
 * @brief:  Enable sampling of program counter via a irq..
 *          Call _start() afterwards.
 *  
 * - IRQ is registered on irqtestperipheral/IRQ2. Make this
 *   hw is not used otherwise
 * - Automatically dumps output to console. Avoid other 
 *   output
 *  
 */

void profiler_enable(u32_t period_cyc){
    struct DrvValue_uint reg_num = {.payload=UINT32_MAX};
	struct DrvValue_uint reg_period = {.payload=period_cyc};	

	irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_PERIOD, &reg_period);

    irqtester_fe310_register_callback(g_dev_cp, 2, irq_handler_profiler_1);

}

void profiler_start(){
    irqtester_fe310_fire_2(g_dev_cp);
}

void profiler_stop(){
    struct DrvValue_uint reg_num = {.payload=0};
	struct DrvValue_uint reg_period = {.payload=0};	

	irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_2_PERIOD, &reg_period);

    irqtester_fe310_unregister_callback(g_dev_cp, 2);

}

#endif // TEST_MINIMAL