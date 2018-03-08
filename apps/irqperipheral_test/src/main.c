
#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>

#include "irqtestperipheral.h"
#include "tests/test_runners.h"
#include "tests/tests.h"
#include "cycles.h"
#include "log_perf.h"

#include "state_manager.h"
#ifndef TEST_MINIMAL
#include <string.h>
#endif
#include "state_machines/sm1.h"

#define IRQTESTER_DRV_NAME "irqtester0"
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

// make static to keep handle after main thread is done
static struct device *dev;

void print_kernel_info(){

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
}



void main(void)
{	
	//dbg1();
	print_kernel_info();
	print_device_drivers();
	printk("Starting irqtester test for hw rev. %i on arch: %s\n", IRQTESTER_HW_REV, CONFIG_ARCH);

	// get driver handles
	dev = device_get_binding(IRQTESTER_DRV_NAME);
	//dbg1();

	if (!dev) {
		printk("Cannot find %s!\n", IRQTESTER_DRV_NAME);
		return;
	}

	irqtester_fe310_enable(dev);
	print_set_verbosity(1);

	//test_uint_overflow();
	run_test_hw_basic_1(dev);	

	//run_test_timing_rx(dev);

	// test basic functionality

	//run_test_irq_throughput_1(dev);
	//run_test_irq_throughput_2(dev);

	// todo: buggy on zc706 (not fe310)
	//run_test_irq_throughput_3_autoadj(dev);
	//PRINT_LOG_BUFF();

	//run_test_poll_throughput_1_autoadj(dev);
	
	// todo: on zc706: fails if not run test_hw_basic_1 first!?
	//run_test_state_mng_1(dev);
	//run_test_state_mng_2(dev);
	//run_test_sm1_throughput_1(dev);
	//PRINT_LOG_BUFF();
	//run_test_sm_throughput_2(dev, 1);
	//run_test_sm_throughput_2(dev, 2);
	//run_test_sm2_action_perf_3(dev);
	//run_test_sm2_action_prof_4(dev);
	PRINT_LOG_BUFF();
	

	int i=0;
	bool abort = false;
	do{
		i++;
		printk("Entering cycle %i. Global max %i \n", i, global_max_cyc);

		
		// timing
		//run_test_timing_rx(dev);
		//run_test_min_timing_rx(dev);
		//run_test_state_mng_1(dev);
		run_test_state_mng_2(dev);

	}while(!abort);
	

	printk("Total uptime / init time  %u ms \n", k_uptime_get_32());
	printk("***** Main thread: I'm done here... Goodbye! ***** \n");
	while(1){
		k_yield();
		// for some reason crashes when exiting main thread, must define threads in main?
	}
	return; // end main thread
}

