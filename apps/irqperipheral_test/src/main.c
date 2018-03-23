
#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>

#include "irqtestperipheral.h"
#include "tests/test_runners.h"
#include "tests/tests.h"
#include "cycles.h"
#include "log_perf.h"
#include "utils.h"

#include "state_manager.h"
#ifndef TEST_MINIMAL
#include <string.h>
#endif
#include "state_machines/sm1.h"


#include <uart.h>

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
	printk("Boot finished at rtc / cycles: [%u / %u] \n", t_rtc, t_cyc);
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

// make static to keep handle after main thread is done
static struct device *d_irqt;
static struct device *d_uart0;
static struct device *d_uart1;
void load_drivers(){
	// get driver handles
	d_irqt = device_get_binding(IRQTESTER_DRV_NAME);

	d_uart0 = device_get_binding(UART1_DRV_NAME);
	d_uart1 = device_get_binding(UART2_DRV_NAME);

	if (!d_irqt) {
		printk("Cannot find driver %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	if (!d_uart0) {
		printk("Cannot find driver %s!\n", UART1_DRV_NAME);
		return;
	}
	if (!d_uart1) {
		printk("Cannot find driver %s!\n", UART2_DRV_NAME);
		return;
	}

}

void uart_print_rx_buf(struct device * dev){
	unsigned char char_in;
	if(0 == uart_poll_in(dev, &char_in)){
		unsigned char name[6];
		if(dev == d_uart0) strcpy(&name, "UART0");
		else if(dev == d_uart1) strcpy(&name, "UART1");
		else strcpy(&name, "UARTX");

		printk("%s received: %c", name, char_in);
		while(0 == uart_poll_in(dev, &char_in)){
			printk("%c", char_in);
		}
		printk("\n");
	}
	
}

void uart_test1(){
	int j=48;  // ASCII: 0
	printk("UART0 Poll out test: \n");
	while(1){

		unsigned char write_char = j;
		
		if(write_char + j < 126){	// last printable char in ASCII
			// repeat char
			for(int i=0; i<10; i++){
				uart_poll_out(d_uart0, write_char);
				k_sleep(200);
				printk("%c", write_char);
			}
			printk("\n");
			uart_print_rx_buf(d_uart1);	// print all rx
			uart_print_rx_buf(d_uart0);	// print all rx
			j++;
		}
		else
			j = 32; // first printable char in ASCII
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



	print_kernel_info();
	print_device_drivers();
	load_drivers();

	uart_test1();

	printk("Starting irqtester test for hw rev. %i on arch: %s\n", IRQTESTER_HW_REV, CONFIG_ARCH);


	

	irqtester_fe310_enable(d_irqt);
	print_set_verbosity(1);
	

	//test_uint_overflow();
	// todo: irqt hardware doesn't work without test here
	run_test_hw_basic_1(d_irqt);	

	//run_test_timing_rx(d_irqt);

	// test basic functionality

	//run_test_irq_throughput_1(d_irqt);
	//run_test_irq_throughput_2(d_irqt);

	// todo: buggy on zc706 (not fe310)
	//run_test_irq_throughput_3_autoadj(d_irqt);
	//PRINT_LOG_BUFF();

	//run_test_poll_throughput_1_autoadj(d_irqt);
	

	// TODO: on zc706: fails if not run test_hw_basic_1 first!?
	//run_test_state_mng_1(d_irqt);
	//run_test_state_mng_2(d_irqt);
	//run_test_sm1_throughput_1(d_irqt);
	//PRINT_LOG_BUFF();
	//run_test_sm_throughput_2(d_irqt, 1);
	//run_test_sm_throughput_2(d_irqt, 2);
	//run_test_sm2_action_perf_3(d_irqt);
	//run_test_sm2_action_prof_4(d_irqt);
	//PRINT_LOG_BUFF();
	

	int i=0;
	bool abort = false;
	do{
		i++;
		printk("Entering cycle %i. Global max %i \n", i, global_max_cyc);

		
		// timing
		//run_test_timing_rx(d_irqt);
		//run_test_min_timing_rx(d_irqt);
		//run_test_state_mng_1(d_irqt);
		run_test_state_mng_2(d_irqt);

	}while(!abort);
	

	printk("Total uptime / init time  %u ms \n", k_uptime_get_32());
	printk("***** Main thread: I'm done here... Goodbye! ***** \n");
	while(1){
		k_yield();
		// for some reason crashes when exiting main thread, must define threads in main?
	}
	return; // end main thread
}

