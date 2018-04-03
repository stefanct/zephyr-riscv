
#ifdef TEST_MINIMAL
#warning "Building test_minimal.c"

// stripped down copy of tests
// uses same header!

#include "utils.h"
#include "irqtestperipheral.h"
#include "tests.h"
#include "cycles.h"

// ugly globals
int error_count = 0;
int error_stamp = 0;

// redefine printk to throw out all unnecessarz prints
#define printk_force(fmt, ...) printk(fmt, ##__VA_ARGS__)
#define printk_(fmt, ...) // deleted


/*
 * Warning: test only with one interrupt fired.
 * Performance will deteriorate if flooded with interrupts and events.
 */
K_MSGQ_DEFINE(drv_q_rx, sizeof(struct DrvEvent), 10, 4);
void test_rx_timing(struct device * dev, int timing_res[], int num_runs, int mode, int verbose){

   	
	bool use_queue = false;
	bool use_fifo = false;
	bool use_semArr = false;
	bool use_valflag = false;
	bool use_valflag_plus_queue = false;
	switch(mode){
		case 0: use_queue = true;
			break;
		case 3: use_valflag = true;
			break;
		case 4: use_valflag_plus_queue = true;
			break;
		default: printk_("Error: unknown mode %i", mode);
		 	test_assert(0);
			return;
	}
	
	printk_("DEBUG: queue: %i, fifo: %i, semArr:%i , valflag: %i, valflag plus queue: %i \n",
				use_queue, use_fifo, use_semArr, use_valflag, use_valflag_plus_queue);
	if(use_valflag + use_queue + use_fifo + use_semArr + use_valflag_plus_queue != 1){
		printk_("ERROR: Invalid mode %i \n", mode);
		test_assert(0);
		return;
	}
	irqtester_fe310_purge_rx(dev);

	if(use_queue){
		irqtester_fe310_register_queue_rx(dev, &drv_q_rx);
		irqtester_fe310_enable_queue_rx(dev);
	}
	if(use_valflag){
		irqtester_fe310_enable_valflags_rx(dev);
	}
	if(use_valflag_plus_queue){
		irqtester_fe310_enable_valflags_rx(dev);
		irqtester_fe310_register_queue_rx(dev, &drv_q_rx);
		irqtester_fe310_enable_queue_rx(dev);
	}

	u32_t delta_cyc;

	for(u32_t i=0; i<num_runs; i++){
		irqtester_fe310_set_value(dev, i);
	
		if(use_queue){			
			u32_t start_cyc = get_cycle_32();
			irqtester_fe310_fire(dev);
			
			struct DrvEvent evt;			
			// short version, just measure delay till first message received
			if(0 != k_msgq_get(&drv_q_rx, &evt, 100)){
				printk_("Message got lost");
				test_assert(0);
				continue;
			}
			delta_cyc = get_cycle_32() - start_cyc;
			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, &evt);
			k_msgq_purge(&drv_q_rx);  // throw away all but first msg

		}

		if(use_valflag){
			// not event driven
			// just checks whether a specified value was loaded into driver mem
			u32_t start_cyc = get_cycle_32();
			int timeout = 10; // shouldn't occur

			//printk_("DEBUG Flags: [enable | perval]: %i|%i\n", 
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_ENABLE),
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL));

			irqtester_fe310_fire(dev);

			while(timeout > 0 && (0 == irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL))){
				timeout--; // high precision busy wait
			}
			delta_cyc = get_cycle_32() - start_cyc;
			if(timeout <= 0){
				printk_("Message got lost \n");
				test_assert(0);
				continue;
			}
			
			//printk_("DEBUG Flags: [enable | perval]: %i|%i\n", 
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_ENABLE),
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL));

			// need to clear flag
			irqtester_fe310_clear_all_valflags(dev);

			if(verbose>1){
				struct DrvValue_uint val;
				irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val);
				printk_("Polled value (id: %i): %i updated at %u ns \n", val._super.id_name, val.payload, val._super.time_ns);
			}
		}

		if(use_valflag_plus_queue){
			int timeout = 10; // shouldn't occur
			u32_t start_cyc = get_cycle_32();
		
			irqtester_fe310_fire(dev);
			struct DrvEvent evt;			

			if(0 != k_msgq_get(&drv_q_rx, &evt, 100)){
				printk_("Queue message got lost");
				test_assert(0);
				continue;
			}

			while(timeout > 0 && (0 == irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL))){
				timeout--; // high precision busy wait
			}
			delta_cyc = get_cycle_32() - start_cyc;

			if(timeout <= 0){
				printk_("Valflag message got lost \n");
				test_assert(0);
			}
			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, &evt);

			// throw away all but first msg
			k_msgq_purge(&drv_q_rx);  
			// need to clear flag
			irqtester_fe310_clear_all_valflags(dev);

			if(verbose>1){
				struct DrvValue_uint val;
				irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val);
				printk_("Polled value (id: %i): %i updated at %u ns \n", val._super.id_name, val.payload, val._super.time_ns);
			}
		}

		// check value
		struct DrvValue_uint val_check;
		irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val_check);
		test_assert(val_check.payload == i);

		int t_res = delta_cyc;;

		// catch timer overflow
		if(delta_cyc < 0){
			printk_("Time overflow detected, discarding value");
			t_res = -1;
		}

		// save or only print timing val
		if(timing_res != NULL){
			timing_res[i] = delta_cyc;
		}
		else if(verbose > 0)
			printk_("%i, ", t_res);
		
	}

	warn_on_new_error();
	irqtester_fe310_reset_hw(dev);
}




int find_min_in_arr(int arr[], int len, int * pos){
	int min = arr[0];
	*pos = 0;

    for(int i=0; i < len; i++){
        if (arr[i] < min){
           min = arr[i];
           *pos = i;
        }
    } 

	return min;
}


int find_max_in_arr(int arr[], int len, int * pos){
	int max = arr[0];
	*pos = 0;

    for(int i=0; i < len; i++){
        if (arr[i] > max){
           max = arr[i];
           *pos = i;
        }
    } 

	return max;
}

// warning: integer division, might be inprecise
int calc_avg_arr(int arr[], int len, bool disc_negative){

	int sum = 0;

	if (len == 0)	// catch zero div
		return 0;

	for(int i=0; i < len; i++){
		if(disc_negative){
			if(arr[i] < 0)
				continue;
		}
		sum += arr[i]; 
    } 

	return sum / len;
}

extern int global_max_cyc;
void print_analyze_timing(int timing[], int len, int verbosity){
	
	int delta_min = find_min_in_arr(timing, len, 0);
	int delta_max = find_max_in_arr(timing, len, 0);
	if(delta_max > global_max_cyc)
		global_max_cyc = delta_max;
	int delta_avg = calc_avg_arr(timing, len, true); // ignore timer overflow vals

	printk_force("Reaction out of %i runs in cycles [avg/min/max]: %i/%i/%i \n",
		 len, delta_avg, delta_min, delta_max);
	printk_force("Reaction out of %i runs in ns [avg/min/max]: %i/%i/%i \n",
		 len, SYS_CLOCK_HW_CYCLES_TO_NS(delta_avg), 
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_min),
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_max));
	if(verbosity > 0){
		printk_force("Detailed reaction in cycles: \n {[");
		for(int i=0; i < len; i++){
			printk_force("%i, ", timing[i]);
		}
		printk_force("]} \n");
	}
}




#endif
