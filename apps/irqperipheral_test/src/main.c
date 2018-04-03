
#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>

#include "globals.h"
#include "irqtestperipheral.h"
#include "tests/test_runners.h"
#include "tests/tests.h"
#include "cycles.h"
#include "log_perf.h"
#include "utils.h"
#include "globals.h"

#include "state_manager.h"
#ifndef TEST_MINIMAL
#include <string.h>
#endif
#include "state_machines/sm1.h"




// set driver names in Kconfig
#define IRQTESTER_DRV_NAME "irqtester0"
#define UART1_DRV_NAME "uart0"			// 1st uart on PMOD J58 header
#define UART2_DRV_NAME "uart1"			// 2nd uart on PMOD J58 header
#define IRQTESTER_HW_REV 3


int global_max_cyc;


// copy of disabled function in devices.c
extern struct device __device_init_end[];
extern struct device __device_init_start[];
void device_list_get(struct device **device_list, int *device_count)
{

	*device_list = __device_init_start;
	*device_count = __device_init_end - __device_init_start;
}

// attention: uses "heavy" strcmp
void print_device_drivers(){
	#ifndef TEST_MINIMAL
	struct device *dev_list_buf;
	int len;

	device_list_get(&dev_list_buf, &len);

	printk("Found %i device drivers: \n", len);
	for(int i=0; i<len; i++){
		char * name = dev_list_buf[i].config->name;
		printk("%s @ %p, ", (strcmp("", name) == 0 ? "<unnamed>" : name), dev_list_buf[i].config->config_info);
	}
	printk("\n");
	#endif
}


void print_kernel_info(){
	#ifndef TEST_MINIMAL
	#ifndef CONFIG_PREEMPT_ENABLED
	#define PRINT_CONFIG_PREEMPT_ENABLED 0
	#else
	#define PRINT_CONFIG_PREEMPT_ENABLED CONFIG_PREEMPT_ENABLED
	#endif
	#ifndef CONFIG_TIMESLICING
	#define PRINT_CONFIG_TIMESLICING 0
	#else
	#define PRINT_CONFIG_TIMESLICING CONFIG_TIMESLICING
	#endif
	#ifndef CONFIG_IRQ_OFFLOAD
	#define PRINT_CONFIG_IRQ_OFFLOAD 0
	#else
	#define PRINT_CONFIG_IRQ_OFFLOAD CONFIG_IRQ_OFFLOAD
	#endif
	u32_t t_rtc = k_cycle_get_32();
	u32_t t_cyc = get_cycle_32();
	printk("Boot finished on %s at rtc / cycles: [%u / %u] \n", CONFIG_ARCH, t_rtc, t_cyc);
	printk("Kernel Config Info:" \
		"\nMain thread @%p prio: %i \nTimeslicing: %i \nPreempt: %i \nIRQ_offloading: %i\n", 
		k_current_get(), CONFIG_MAIN_THREAD_PRIORITY, PRINT_CONFIG_TIMESLICING,
		PRINT_CONFIG_PREEMPT_ENABLED, PRINT_CONFIG_IRQ_OFFLOAD);
	
	// driver specific
	#if CONFIG_FE310_IRQT_DRIVER
	printk("isr.S optimization: %i\n", CONFIG_FE310_ISR_PLIC_OPT_LVL);
	#endif
	#endif // TEST_MINIMAL
}

// vars main declaration in globals.h, define here
struct device * g_dev_irqt;
struct device * g_dev_uart0;
struct device * g_dev_uart1;
void load_drivers(){
	// get driver handles
	g_dev_irqt = device_get_binding(IRQTESTER_DRV_NAME);

	g_dev_uart0 = device_get_binding(UART1_DRV_NAME);
	g_dev_uart1 = device_get_binding(UART2_DRV_NAME);

	if (!g_dev_irqt) {
		printk("Cannot find driver %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	if (!g_dev_uart0) {
		printk("Cannot find driver %s!\n", UART1_DRV_NAME);
		return;
	}
	if (!g_dev_uart1) {
		printk("Cannot find driver %s!\n", UART2_DRV_NAME);
		return;
	}

}



void main(void)
{	
	/**
	 * Printing policy:
	 * - available: 
	 * a) logging (SYS_LOG_<>), b) (verbsoity controlled) printkv, c) buffered LOG_PERF
	 * - usage:
	 * a) stuff that runs periodcially in productive coe
	 * 	  -> can be thrown out of code with kconfig
	 * b) stuff in tests that is not run periodiclly
	 * 	  -> can control vai print_set_verbosity 
	 *    -> stays in binary, even when vebosity set to not show
	 * 	  -> productive code should avoid compiling test code anyway
	 * c) stuff inside periodically run code
	 *    -> logs to buffer and prints after run
	 *    -> still slower than not printing, better than serial out
	 *    -> set CONFIG_APP_LOG_PERF_BUFFER_DEPTH=0 to deactivate
	 */

	bool quick_test = true;

	print_kernel_info();
	print_device_drivers();
	load_drivers();

	print_set_verbosity(1);

	/* rx/tx test for uart0 */
	test_uart_1(g_dev_uart0, g_dev_uart1);
	
	printk("Starting irqtester test for hw rev. %i\n", IRQTESTER_HW_REV);
	irqtester_fe310_enable(g_dev_irqt);
	
	/* basic irqt hardware check */
	run_test_hw_basic_1(g_dev_irqt);	
	/* isr up-passing */
	if(!quick_test)
		run_test_timing_rx(g_dev_irqt);

	/* irq troughput tests (inaccurate!) */
	if(!quick_test){
		run_test_irq_throughput_1(g_dev_irqt);
		run_test_irq_throughput_2(g_dev_irqt);
	}
	run_test_irq_throughput_3_autoadj(g_dev_irqt);
	

	/* state_manager basic test */
	if(!quick_test)
		run_test_state_mng_1(g_dev_irqt);

	/* state_manager throughputs (inccurate!) */
	//run_test_sm1_throughput_1(g_dev_irqt);	// irq1 (reset) throughput
	//run_test_sm_throughput_2(g_dev_irqt, 1);	// irq2 (val updt) throughput for sm1
	//run_test_sm_throughput_2(g_dev_irqt, 2);	// irq2 (val updt) throughput for sm2
	
	/* benchmark of state_manager, used for execution time model calibration */
	//run_test_sm2_action_perf_3(g_dev_irqt);

	/* used for profiling state_manager */
	//run_test_sm2_action_prof_4(g_dev_irqt);
	//PRINT_LOG_BUFF();
	

	/* continous bench timing of state_manager critical loop */
	int i=0;
	int n_runs = 10;
	global_max_cyc = -1;
	print_set_verbosity(2);
	do{
		i++;
		run_test_state_mng_2(g_dev_irqt);
		printk("[%i] main() loop, global max %i \n", i, global_max_cyc);

	}while(i < n_runs);
	

	printk("Total uptime / init time  %u ms \n", k_uptime_get_32());
	printk("***** Main thread: I'm done here... Goodbye! ***** \n");
	while(1){
		k_yield();
	}
	return; // end main thread
}

