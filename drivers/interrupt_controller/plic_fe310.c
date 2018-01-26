/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Platform Level Interrupt Controller (PLIC) driver
 *        for the SiFive Freedom E310 processor
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <init.h>
#include <soc.h>
#include <sw_isr_table.h>
// for perf profiling
#include "../../programs_timo/irqperipheral_test/src/log_perf.h"
#include "../../programs_timo/irqperipheral_test/src/cycles.h"
//#include "../../programs_timo/irqperipheral_test/src/tests/tests.h"

#define PLIC_FE310_IRQS        (CONFIG_NUM_IRQS - RISCV_MAX_GENERIC_IRQ)
#define PLIC_FE310_EN_SIZE     ((PLIC_FE310_IRQS >> 5) + 1)

struct plic_fe310_regs_t {
	u32_t threshold_prio;
	u32_t claim_complete;
};

static int save_irq;

/**
 *
 * @brief Enable a riscv PLIC-specific interrupt line
 *
 * This routine enables a RISCV PLIC-specific interrupt line.
 * riscv_plic_irq_enable is called by SOC_FAMILY_RISCV_PRIVILEGE
 * _arch_irq_enable function to enable external interrupts for
 * IRQS > RISCV_MAX_GENERIC_IRQ, whenever CONFIG_RISCV_HAS_PLIC
 * variable is set.
 * @param irq IRQ number to enable
 *
 * @return N/A
 */
void riscv_plic_irq_enable(u32_t irq)
{
	u32_t key;
	u32_t fe310_irq = irq - RISCV_MAX_GENERIC_IRQ;
	volatile u32_t *en =
		(volatile u32_t *)FE310_PLIC_IRQ_EN_BASE_ADDR;

	key = irq_lock();
	en += (fe310_irq >> 5);
	*en |= (1 << (fe310_irq & 31));
	irq_unlock(key);
}

/**
 *
 * @brief Disable a riscv PLIC-specific interrupt line
 *
 * This routine disables a RISCV PLIC-specific interrupt line.
 * riscv_plic_irq_disable is called by SOC_FAMILY_RISCV_PRIVILEGE
 * _arch_irq_disable function to disable external interrupts, for
 * IRQS > RISCV_MAX_GENERIC_IRQ, whenever CONFIG_RISCV_HAS_PLIC
 * variable is set.
 * @param irq IRQ number to disable
 *
 * @return N/A
 */
void riscv_plic_irq_disable(u32_t irq)
{
	u32_t key;
	u32_t fe310_irq = irq - RISCV_MAX_GENERIC_IRQ;
	volatile u32_t *en =
		(volatile u32_t *)FE310_PLIC_IRQ_EN_BASE_ADDR;

	key = irq_lock();
	en += (fe310_irq >> 5);
	*en &= ~(1 << (fe310_irq & 31));
	irq_unlock(key);
}

/**
 *
 * @brief Check if a riscv PLIC-specific interrupt line is enabled
 *
 * This routine checks if a RISCV PLIC-specific interrupt line is enabled.
 * @param irq IRQ number to check
 *
 * @return 1 or 0
 */
int riscv_plic_irq_is_enabled(u32_t irq)
{
	volatile u32_t *en =
		(volatile u32_t *)FE310_PLIC_IRQ_EN_BASE_ADDR;
	u32_t fe310_irq = irq - RISCV_MAX_GENERIC_IRQ;

	en += (fe310_irq >> 5);
	return !!(*en & (1 << (fe310_irq & 31)));
}

/**
 *
 * @brief Set priority of a riscv PLIC-specific interrupt line
 *
 * This routine set the priority of a RISCV PLIC-specific interrupt line.
 * riscv_plic_irq_set_prio is called by riscv32 _ARCH_IRQ_CONNECT to set
 * the priority of an interrupt whenever CONFIG_RISCV_HAS_PLIC variable is set.
 * @param irq IRQ number for which to set priority
 *
 * @return N/A
 */
void riscv_plic_set_priority(u32_t irq, u32_t priority)
{
	volatile u32_t *prio =
		(volatile u32_t *)FE310_PLIC_PRIO_BASE_ADDR;

	/* Can set priority only for PLIC-specific interrupt line */
	if (irq <= RISCV_MAX_GENERIC_IRQ)
		return;

	if (priority > FE310_PLIC_MAX_PRIORITY)
		priority = FE310_PLIC_MAX_PRIORITY;

	prio += (irq - RISCV_MAX_GENERIC_IRQ);
	*prio = priority;
}

/**
 *
 * @brief Get riscv PLIC-specific interrupt line causing an interrupt
 *
 * This routine returns the RISCV PLIC-specific interrupt line causing an
 * interrupt.
 * @param irq IRQ number for which to set priority
 *
 * @return N/A
 */
int riscv_plic_get_irq(void)
{
	return save_irq;
}

static inline void reg_read_uint(uintptr_t addr, uint32_t * data)
{
	*data = *((volatile uint32_t *) addr);
}

static inline void reg_write_short(uintptr_t addr, uint8_t data)
{
	volatile uint8_t *ptr = (volatile uint8_t *) addr;
	*ptr = data;
}

/*
#define IRQ1_CLEAR  0x201C
#define IRQ1_STATUS  0x2018
// debug handler only for throughput test
// optimal case: isr.S directly calls this function 
// without irq dispatching
// must have access to vars, make extern in tests.h
void plic_fe310_irq_handler_dbg(void *arg){
	//printk("Fake deep handler called \n");
	

	if(i_handler < _status_arr_length){
		u32_t status_new = 0;
        // read error status
        reg_read_uint(IRQ1_STATUS, &status_new);
        // save how many irqs missed since handler called alst time
        _status_arr[i_handler] = status_new - status_stamp;
        // debug
        //printk("Saving %u (new %u, stamp %u) errors in i_handler run %i, i_arr %i \n", status_new - status_stamp, status_new, status_stamp,  i_handler, i_handler);
        status_stamp = status_new;
    }
	else{
		k_sem_give(&handler_sem);
	}
	
	i_handler++;
	reg_write_short(IRQ1_CLEAR, 1);
    reg_write_short(IRQ1_CLEAR, 0);

	volatile struct plic_fe310_regs_t *regs =
		(volatile struct plic_fe310_regs_t *)FE310_PLIC_REG_BASE_ADDR;

	u32_t irq = regs->claim_complete; // cleared upon read
	regs->claim_complete = irq;

	// we pass the irq_num in reg a6
	//u32_t a0;
	//u32_t t;
	// to have in some reg, add 0 to target reg
	//__asm__ volatile("addi %0, a6, 0" : "=r" (a0));
	//__asm__ volatile("addi %0, t5, 0" : "=r" (t));
	//printk("[%i] reg a0: %u, t4: %u irq %u \n", i_handler, (int)a0, (int)t, irq);
}

#define IRQ0_PERVAL  0x2004
// debug handler only for latency test
void plic_fe310_irq_handler_dbg_2(void){
	//LOG_PERF("[%u] Entering irq_handler_mes_time", get_cycle_32());

	time_isr =  get_cycle_32();
	reg_read_uint(IRQ0_PERVAL, &isr_perval);

	k_sem_give(&wait_sem);
	
	volatile struct plic_fe310_regs_t *regs =
	(volatile struct plic_fe310_regs_t *)FE310_PLIC_REG_BASE_ADDR;

	u32_t irq = regs->claim_complete; // cleared upon read
	regs->claim_complete = irq;

}
*/

/**
 * @brief: ISR for every global IRQ on the PLIC line.
 * 
 * Is placed by CONNECT() macro to correct position in _sw_isr_table.
 * Calculates the offset to functions on external lines connected to PLIC
 * and also placed in _sw_isr_table. 
 */
static void plic_fe310_irq_handler(void *arg)
{
	//LOG_PERF_INT(1, get_cycle_32());

	volatile struct plic_fe310_regs_t *regs =
		(volatile struct plic_fe310_regs_t *)FE310_PLIC_REG_BASE_ADDR;

	u32_t irq;
	struct _isr_table_entry *ite;

	/* Get the IRQ number generating the interrupt */
	irq = regs->claim_complete;

	/*
	 * Save IRQ in save_irq. To be used, if need be, by
	 * subsequent handlers registered in the _sw_isr_table table,
	 * as IRQ number held by the claim_complete register is
	 * cleared upon read.
	 */
	save_irq = irq;

	/*
	 * If the IRQ is out of range, call _irq_spurious.
	 * A call to _irq_spurious will not return.
	 */
	if (irq == 0 || irq >= PLIC_FE310_IRQS)
		_irq_spurious(NULL);

	irq += RISCV_MAX_GENERIC_IRQ;

	/* Call the corresponding IRQ handler in _sw_isr_table */
	ite = (struct _isr_table_entry *)&_sw_isr_table[irq];
	ite->isr(ite->arg);

	/*
	 * Write to claim_complete register to indicate to
	 * PLIC controller that the IRQ has been handled.
	 */
	regs->claim_complete = save_irq;
}

/**
 * 	@brief: Handler directly called from isr.S on plic irq 
 * 			while bypassing sw_isr_table.
 *  		Use in combination with connect_handler_fast()
 */
static void(*_isr_fast)(int num_irq) = _irq_spurious;
void plic_fe310_irq_handler_fast(void *arg){

	volatile struct plic_fe310_regs_t *regs =
		(volatile struct plic_fe310_regs_t *)FE310_PLIC_REG_BASE_ADDR;

	u32_t irq_num = regs->claim_complete; // cleared upon read
	// to support subsequent callbacks to use eg. riscv_plic_get_irq()
	save_irq = irq_num; 

	// invoke registered callback
	_isr_fast(irq_num);

	// indicate to plic that irq has been handled
	regs->claim_complete = irq_num;
	//printk("Claim_complete at irq %i \n", irq_num);

	// debug: to read reg, could also be done by "mv %0, t5"
	//u32_t t;
	//u32_t t2;
	//__asm__ volatile("addi %0, t1, 0" : "=r" (t));
	//__asm__ volatile("addi %0, t2, 0" : "=r" (t2));
	//printk("t1 %u t2 %u \n", t1, t2);
}

/**
 * @brief: Register handler for fast interrupt handling of plic irq.
 * 
 * @param irq_handler: Callback function invoked on plic interrupt.
 * 					   Must take number of irq line as argument and
 * 					   dispatch different sources.
 */
void plic_fe310_connect_handler_fast(void (*irq_handler)(int num_irq)){
	_isr_fast = irq_handler;
}

/**
 *
 * @brief Initialize the SiFive FE310 Platform Level Interrupt Controller
 * @return N/A
 */
static int plic_fe310_init(struct device *dev)
{
	ARG_UNUSED(dev);

	volatile u32_t *en =
		(volatile u32_t *)FE310_PLIC_IRQ_EN_BASE_ADDR;
	volatile u32_t *prio =
		(volatile u32_t *)FE310_PLIC_PRIO_BASE_ADDR;
	volatile struct plic_fe310_regs_t *regs =
		(volatile struct plic_fe310_regs_t *)FE310_PLIC_REG_BASE_ADDR;
	int i;

	/* Ensure that all interrupts are disabled initially */
	for (i = 0; i < PLIC_FE310_EN_SIZE; i++) {
		*en = 0;
		en++;
	}

	/* Set priority of each interrupt line to 0 initially */
	for (i = 0; i < PLIC_FE310_IRQS; i++) {
		*prio = 0;
		prio++;
	}

	/* Set threshold priority to 0 */
	regs->threshold_prio = 0;

	/* Setup IRQ handler for PLIC driver */
	// if _OPT_LVL > 0 called directly from isr.S,  don't install in sw_isr_table
	IRQ_CONNECT(RISCV_MACHINE_EXT_IRQ,
		    FE310_PLIC_MAX_PRIORITY, 	// t_dev: original: 0
		#if CONFIG_FE310_ISR_PLIC_OPT_LVL > 0
		    _irq_spurious,
		#else
			plic_fe310_irq_handler,
		#endif
		    NULL,
		    0);
	
	/* Enable IRQ for PLIC driver */
	irq_enable(RISCV_MACHINE_EXT_IRQ);

	return 0;
}

SYS_INIT(plic_fe310_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
