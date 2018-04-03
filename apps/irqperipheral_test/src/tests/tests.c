#ifndef TEST_MINIMAL

#include "../utils.h"
#include "irqtestperipheral.h"
#include "../state_manager.h"
#include "tests.h"
#include "cycles.h"
#include "log_perf.h"
#include "test_suite.h"



// running average
// warning: only integer division right now, inprecise
int getAvg (int x, bool reset){
    static int sum, n;
	
	if(reset){	
		// reset static vars
		// use if you call for > 1 average calcs
		sum = 0;
		n = 0;
	}

    sum += x;
    return (((int)sum)/++n);
}

void test_uint_overflow(){
	print_dash_line(0);
	printk_framed(0, "Now checking uint (timer) overrroll behaviour ");
    print_dash_line(0);

	printk("1: (non overroll) UINT_MAX - (UINT_MAX-1) \n2: (overroll) UINT_MAX + 10 - (UINT_MAX-1) \n");
	printk("Expect (1) + 10 == (2) \n");
	u32_t high_normal = UINT_MAX;
	u32_t high_ov = UINT_MAX + 10;
	u32_t low = UINT_MAX -1 ;

	u32_t dif_normal = high_normal - low;
	u32_t dif_ov = high_ov - low;

	test_assert(dif_normal + 10 == dif_ov );
	printk("dif normal: %u, overroll: %u \n", dif_normal, dif_ov);
	test_print_report(0);
}

// tests functionality of hw rev 1
// compatible with hw revs: 1,2,3
void test_hw_rev_1_basic_1(struct device * dev){
	// test generic reg setters and getters, driver mem getter
	struct DrvValue_uint val = {.payload=42}; 
	struct DrvValue_uint test;
	struct DrvValue_bool enable; 
	struct DrvValue_uint val2;

	// save enable status on enter
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	bool enable_on_enter = enable.payload;
	printkv(2, "get_reg enable0: %i, should be 1\n", enable_on_enter);
	test_assert(enable_on_enter == 1);

	// without load to driver, value should be 0 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);	
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printkv(2, "get_reg perval0: %i, should be 0\n", test.payload);
	test_assert(test.payload == (u32_t)0);

	// firing loads register into driver values
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printkv(2, "get_val perval0: %i, should be 42 \n", test.payload);
	test_assert(test.payload == (u32_t)42);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_STATUS, &test);
	printkv(2, "get_reg status0: %i, should be 0 \n", test.payload);
	test_assert(test.payload == (u32_t)0); // no error code status

	// disabling won't fire the interrupt, so nothing loaded
	enable.payload =false; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	val2.payload = 7; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val2);
	irqtester_fe310_fire(dev);
	// load from memory value
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printkv(2, "get_val perval0: %i, should be 42 \n", test.payload);
	test_assert(test.payload == (u32_t)42);
	
	// load from register should still work after re-enabling
	enable.payload=true; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &test);
	test_assert(test.payload == (u32_t)7);
	printkv(2, "get_reg perval0: %i, should be 7 \n", test.payload);
	/* disable, will always fail on usual setting CONFIG_IRQTESTER_FE310_FAST_ID2IDX

	printkv(2, "get_reg perval0: %i \n", test.payload);
	printkv(2, "Trying to get invalid value. Expect and ignore error.\nWill fail if set CONFIG_IRQTESTER_FE310_FAST_ID2IDX \n");
	
	int ret = irqtester_fe310_get_val((u32_t)-1, &test);
	test_assert(ret != 0);
	*/

	// clean up
	irqtester_fe310_reset_hw(dev);
	
}

static struct device * dev_cp;
void irq_handler_clear_irq_1(void){
	struct DrvValue_bool val;
	val.payload = 1;
	irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
	u32_t t = get_cycle_32();
	printkv(3, "Interrupt: Cleared at %i cycles \n", t);
	// reset clear input
	val.payload = 0;
	irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
}

// test new DPS blocks 
// compatible with hw revs: > 2
void test_hw_rev_2_basic_1(struct device * dev){

	u32_t NUM_FIRE_1 = 3;
	u32_t PERIOD_FIRE_1 = 100000;  // clock cycles
	dev_cp = dev; // store to static var to have access

	// configure an interrupt
	struct DrvValue_uint reg_num = {.payload=NUM_FIRE_1};
	struct DrvValue_uint reg_period = {.payload=PERIOD_FIRE_1};	
	struct DrvValue_uint val;
	struct DrvValue_uint status_1;
	struct DrvValue_uint status_2;
	struct DrvValue_bool clear_1;

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_2_PERIOD, &reg_period);
	irqtester_fe310_get_reg(dev, VAL_IRQ_2_STATUS, &status_2);

	printkv(2, "get_reg status_1: %i \n", status_1.payload);
	printkv(2, "get_reg status_2: %i \n", status_2.payload);
	
	// fire irq1 3x, with handler that won't clear interrupt
	irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_1);
	irqtester_fe310_register_callback(dev, IRQ_2, _irq_1_handler_1);
	irqtester_fe310_fire_1(dev);
	irqtester_fe310_fire_2(dev);


	u32_t t_sleep_ms = CYCLES_CYC_2_MS(NUM_FIRE_1 * PERIOD_FIRE_1);
	printkv(2, "Sleeping for %i ms \n", t_sleep_ms);
	k_sleep(t_sleep_ms); 
	
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_NUM_REP, &val);

	printkv(2, "get_reg num_rep_1: %i \n", val.payload);
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_PERIOD, &val);

	printkv(2, "get_reg period_1: %i \n", val.payload);
	
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);

	printkv(2, "get_reg status_1: %i, should be %i \n", status_1.payload, NUM_FIRE_1);
	irqtester_fe310_get_reg(dev, VAL_IRQ_2_STATUS, &status_2);

	printkv(2, "get_reg status_2: %i, should be %i \n", status_2.payload, NUM_FIRE_1);

	// we din't clear the interrupt, so status should be iterated
	test_assert(status_1.payload == NUM_FIRE_1);
	test_assert(status_2.payload == NUM_FIRE_1);

	// install own interrupt handler which clears
	irqtester_fe310_register_callback(dev, IRQ_1, irq_handler_clear_irq_1);
	// fire irq1 3x
	
	// debug clear before fire
	//struct DrvValue_bool val2;
	//val2.payload = 1;
	//irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val2);
	// end debbug

	u32_t t = get_cycle_32();
	irqtester_fe310_fire_1(dev);
	k_sleep(10 * CYCLES_CYC_2_MS(NUM_FIRE_1 * PERIOD_FIRE_1));  

	printkv(2, "Fired at %i cycles \n", t);

	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printkv(2, "get_reg status_1: %i, should be %i \n", status_1.payload, NUM_FIRE_1);
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_CLEAR, &clear_1);

	printkv(2, "get_reg clear_1: %i \n", clear_1.payload);

	// we did clear the interrupt, so status should be unchanged
	test_assert(status_1.payload == NUM_FIRE_1);

	irqtester_fe310_reset_hw(dev);

	// reset to default handlers
	irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);
	irqtester_fe310_register_callback(dev, IRQ_2, _irq_gen_handler);

}

void test_hw_rev_3_basic_1(struct device * dev){
	
	u32_t period_poll = 1000000;	// cycles
	u32_t num_poll = 10;
	
	// no need for handlers or even irqs coming through
	irqtester_fe310_unregister_callback(dev, IRQ_0);
	irqtester_fe310_unregister_callback(dev, IRQ_1);
	irqtester_fe310_unregister_callback(dev, IRQ_2);

	// configure an interrupt for irq_1
	struct DrvValue_uint irq_num = {.payload=1};
    
	// configure polling dsp_3 block 
	struct DrvValue_uint delay = {.payload=period_poll};
	struct DrvValue_uint period = {.payload=period_poll};
	struct DrvValue_uint num_rep = {.payload=num_poll};
	struct DrvValue_uint duty = {.payload=period_poll};

	struct DrvValue_uint val;
	struct DrvValue_uint status_1;
	struct DrvValue_uint status_3;

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &irq_num);

	irqtester_fe310_set_reg(dev, VAL_DSP_3_PERIOD, &period);
	irqtester_fe310_set_reg(dev, VAL_DSP_3_DUTY, &duty);
	irqtester_fe310_set_reg(dev, VAL_DSP_3_NUM_REP, &num_rep);
	// delay != 0 will activate polling device
	irqtester_fe310_set_reg(dev, VAL_DSP_3_DELAY, &delay);

	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);


	printkv(2, "get_reg status_1: %i \n", status_1.payload);
	printkv(2, "get_reg status_3: %i \n", status_3.payload);
	
	u32_t t_start = get_cycle_32();
	u32_t t_end;
	irqtester_fe310_fire_1(dev);
	for(u32_t i=0; i<num_poll; i++){
		// check ready with correct ID_COUNTER
		int timeout = period_poll / num_poll;
		bool ready = false;
		u32_t counter_reg;

		while(!ready && timeout >= 0){
			irqtester_fe310_get_reg(dev, VAL_DSP_3_ID_COUNTER, &val);
			counter_reg = val.payload;
			
			struct DrvValue_bool ready_val;
			irqtester_fe310_get_reg(dev, VAL_DSP_3_READY, &ready_val);
			bool ready_reg = ready_val.payload;

			if(ready_reg && i+1 == counter_reg){ // counter_reg starts from 1
				t_end = get_cycle_32();
				ready = true;
			}
			timeout--;
		}
		if(timeout > 0){
			// clear
			val.payload = i+1;
			irqtester_fe310_set_reg(dev, VAL_DSP_3_CLEAR_ID, &val);
			printkv(3, "Successfully polled counter_id %u in t= %u cycles since irq, timeout= %i \n", counter_reg, t_end - t_start, timeout);

		}
		else{
			test_assert(0);
			printkv(2, "Polling value %i timed out \n", i+1);
		}

	}

	irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);
	test_assert(0 == status_3.payload);

	// make sure devie is ready (not still firing) by resetting
	struct DrvValue_bool reset = {.payload=1};
	irqtester_fe310_set_reg(dev, VAL_DSP_3_RESET, &reset);
	reset.payload = 0;
	irqtester_fe310_set_reg(dev, VAL_DSP_3_RESET, &reset);

	// need manually to set to reserved 0 value, otherwise last clear_id persists
	val.payload = 0;
	irqtester_fe310_set_reg(dev, VAL_DSP_3_CLEAR_ID, &val);

	// now fire without clearing poll values -> should count error status
	irqtester_fe310_fire_1(dev);

	for(u32_t i=0; i<num_poll; i++){
		// check ready with correct ID_COUNTER
		int timeout = period_poll / num_poll;
		bool ready = false;
		u32_t counter_reg;

		while(!ready && timeout >= 0){
			irqtester_fe310_get_reg(dev, VAL_DSP_3_ID_COUNTER, &val);
			counter_reg = val.payload;
			
			struct DrvValue_bool ready_val;
			irqtester_fe310_get_reg(dev, VAL_DSP_3_READY, &ready_val);
			bool ready_reg = ready_val.payload;

			if(ready_reg && i+1 == counter_reg)
				ready = true;
			timeout--;
		}
		if(timeout > 0){
			// dont clear
			irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);
			irqtester_fe310_get_reg(dev, VAL_DSP_3_CLEAR_ID, &val);
			printkv(3, "Successfully polled with no clear counter_id %u, status_3 %i, clear_id= %i \n", counter_reg, status_3.payload, val.payload);

		}
		else{
			test_assert(0);
			printkv(2, "Polling value %i timed out \n", i+1);
		}

	}
	// wait untill firing done for sure
	k_sleep(CYCLES_CYC_2_MS(period_poll));

	irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);
	test_assert(num_poll == status_3.payload);

	printkv(2, "get_reg status_3: %i \n", status_3.payload);

	irqtester_fe310_reset_hw(dev);
}




K_SEM_DEFINE(wait_sem, 0, 1);
static struct device * dev_cp;
static u32_t time_isr = 0;
static u32_t isr_perval;
// if need to make global by "extern" in tests.h
// u32_t isr_perval = 0;
// u32_t time_isr = 0;

void irq_handler_mes_time(void){
	//LOG_PERF("[%u] Entering irq_handler_mes_time", get_cycle_32());
	time_isr =  get_cycle_32();

	LOG_PERF_INT(3, time_isr);
	u32_t res_perval;
	irqtester_fe310_get_perval(dev_cp, &res_perval);

	isr_perval = res_perval;
	k_sem_give(&wait_sem);
	//printk("Callback fired at %i ticks. [perval / enable]: %i, %i \n", time_isr, 
	//	res_perval, res_enable);
}

// warning, can't test whether value in ISR is correct
void test_interrupt_timing(struct device * dev, int timing_res[], int num_runs, int verbose){

	dev_cp = dev; // store to static var to have access

	printkv(2, "Make sure that non-default irq handler is installed to driver! \n");

	u32_t delta_cyc;

	for(u32_t i=0; i<num_runs; i++){

		
		struct DrvValue_uint val;
		val.payload = i;
		irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);
		//LOG_PERF("[%u] Firing irq", get_cycle_32());
		LOG_PERF_INT(0, get_cycle_32());
		u32_t start_cyc = get_cycle_32();
		irqtester_fe310_fire(dev);

		// measure time since fire
		// do only for first message, because handling the first
		// messages takes some time - and we don't want to measure this
		if (k_sem_take(&wait_sem, 100) != 0) {
        	printk("Warning: Failed to take semaphore \n");
			test_assert(0);
    	} else {
			// check value
			test_assert(isr_perval == i);
			// save timing
			delta_cyc = time_isr - start_cyc;
			timing_res[i] = delta_cyc;
			// catch timer overflow
			if(delta_cyc < 0){
				printk("Time overflow detected, discarding value");
				timing_res[i] = -1;
				continue; // ignores value
			}
		}

	}

	test_warn_on_new_error();
	irqtester_fe310_reset_hw(dev);

}

/*
 * Warning: test only with one interrupt fired.
 * Performance will deteriorate if flooded with interrupts and events.
 */
K_MSGQ_DEFINE(drv_q_rx, sizeof(struct DrvEvent), 10, 4);
K_FIFO_DEFINE(drv_fifo_rx);
K_SEM_DEFINE(drv_sem_rx, 0, 10);
struct DrvEvent drv_evt_arr_rx[10];
void test_rx_timing(struct device * dev, int timing_res[], int num_runs, int mode, int verbose){

   	
	bool use_queue = false;
	bool use_fifo = false;
	bool use_semArr = false;
	bool use_valflag = false;
	bool use_valflag_plus_queue = false;
	switch(mode){
		case 0: use_queue = true;
			break;
		case 1: use_fifo = true;
			break;
		case 2: use_semArr = true;
			break;
		case 3: use_valflag = true;
			break;
		case 4: use_valflag_plus_queue = true;
			break;
		default: printk("Error: unknown mode %i", mode);
		 	test_assert(0);
			return;
	}
	
	printkv(3, "DEBUG: queue: %i, fifo: %i, semArr:%i , valflag: %i, valflag plus queue: %i \n",
				use_queue, use_fifo, use_semArr, use_valflag, use_valflag_plus_queue);
	if(use_valflag + use_queue + use_fifo + use_semArr + use_valflag_plus_queue != 1){
		printk("ERROR: Invalid mode %i \n", mode);
		test_assert(0);
		return;
	}
	irqtester_fe310_purge_rx(dev);
	if(use_fifo){
		irqtester_fe310_register_fifo_rx(dev, &drv_fifo_rx);
		irqtester_fe310_enable_fifo_rx(dev);
	}
	if(use_queue){
		irqtester_fe310_register_queue_rx(dev, &drv_q_rx);
		irqtester_fe310_enable_queue_rx(dev);
	}
	if(use_semArr){
		irqtester_fe310_register_sem_arr_rx(dev, &drv_sem_rx, drv_evt_arr_rx, 10);
		irqtester_fe310_enable_sem_arr_rx(dev);
	}
	if(use_valflag){
		irqtester_fe310_enable_valflags_rx(dev);
	}
	if(use_valflag_plus_queue){
		irqtester_fe310_enable_valflags_rx(dev);
		irqtester_fe310_register_queue_rx(dev, &drv_q_rx);
		irqtester_fe310_enable_queue_rx(dev);
	}

	u32_t delta_cyc = 0;

	for(u32_t i=0; i<num_runs; i++){
		struct DrvValue_uint val;
		val.payload = i;
		irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);

		if(use_fifo){
			u32_t start_cyc = get_cycle_32();
			irqtester_fe310_fire(dev);

			struct DrvEvent * p_evt;
			struct DrvEvent evt;
			p_evt = k_fifo_get(&drv_fifo_rx, 100);
			evt = *p_evt;	// copy into container to avoid loosing scope

			if(NULL == p_evt){
				printk("Message got lost");
				test_assert(0);
				continue;
			}
			delta_cyc = get_cycle_32() - start_cyc;

			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, p_evt);

			// purge fifo, since only interessted in reaction till first msg
			// workaround for broken fifo
			int i = 0;	
			int i_limit = 10;		
			while(1){
				p_evt = k_fifo_get(&drv_fifo_rx, K_NO_WAIT);
				if(p_evt == NULL || i == i_limit)
					break;
				evt = *p_evt;
				printkv(3, "Discarding fifo element %p. Event [cleared, val_id, val_type, event_type, irq_id]: %i, %i, %i, %i, %i \n" \
					,evt._reserved, evt.cleared, evt.val_id, evt.val_type, evt.event_type, evt.irq_id);
				
				i++;
				; //  actually, nothing to do here
			}
			if(i == i_limit){
				test_assert(0);
				printk("ERROR: Aborted reading fifo, seems hung. \n");
			}

		}
		if(use_semArr){
			u32_t start_cyc = get_cycle_32();
			irqtester_fe310_fire(dev);

			struct DrvEvent evt;
			
			// short version, only first msg
			if(0 != irqtester_fe310_receive_evt_from_arr(dev, &evt, K_MSEC(100)) ){
				printk("Message got lost");
				test_assert(0);
				continue;
			}
			delta_cyc = get_cycle_32() - start_cyc;
			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, &evt);

			// purge array since only interessted in reaction till first msg
			while(0 != irqtester_fe310_receive_evt_from_arr(dev, &evt, K_NO_WAIT)){
				; // nothing to do here
			}
			/*
			k_sem_take(&(data->_sem_rx), timeout)){		
			k_sem_give(&(data->_sem_rx));
			while(k_sem_count_get(&drv_sem_rx) != 0){
				if(k_sem_take(&drv_sem_rx, K_MSEC(100)) != 0){
					printk("Message got lost");
					continue;
				}

				int count = k_sem_count_get(&drv_sem_rx);
				// todo: is this critical?
				struct DrvEvent evt = drv_evt_arr_rx[count];
				// measure time only for first msg, purge rest
				if(i==0)
					delta_cyc = get_cycle_32() - start_cyc;

				if(evt.cleared != 0)
					printk("Event with id %i at count %i shouln't be cleared yet!", evt.id_name, count);
				else{
					drv_evt_arr_rx[count].cleared++;
					if(verbose>1)
						irqtester_fe310_dbgprint_event(dev, &evt);
				}
				i++;
			}
			*/	
		}
		if(use_queue){			
			u32_t start_cyc = get_cycle_32();
			irqtester_fe310_fire(dev);
			
			struct DrvEvent evt;			
			// short version, just measure delay till first message received
			if(0 != k_msgq_get(&drv_q_rx, &evt, 100)){
				printk("Message got lost");
				test_assert(0);
				continue;
			}
			delta_cyc = get_cycle_32() - start_cyc;
			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, &evt);
			k_msgq_purge(&drv_q_rx);  // throw away all but first msg

			/* long version, dbgprint all events in queue
			// wait for new queue element
			int num_msgs = k_msgq_num_used_get(&drv_q_rx);
			for(int i=0; i<num_msgs; i++){			
				if(0 != k_msgq_get(&drv_q_rx, &evt, 100))
					printk("Message got lost");
				else{
					// measure time since fire
					// do only for first message, because handling the first
					// messages takes some time - and we don't want to measure this
					if(i==0){
						delta_cyc = get_cycle_32() - start_cyc;
					}
					//irqtester_fe310_dbgprint_event(dev, &evt);
				}
			}
			*/
		}

		if(use_valflag){
			// not event driven
			// just checks whether a specified value was loaded into driver mem
			u32_t start_cyc = get_cycle_32();
			int timeout = 10; // shouldn't occur

			//printk("DEBUG Flags: [enable | perval]: %i|%i\n", 
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_ENABLE),
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL));

			irqtester_fe310_fire(dev);

			while(timeout > 0 && (0 == irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL))){
				timeout--; // high precision busy wait
			}
			delta_cyc = get_cycle_32() - start_cyc;
			if(timeout <= 0){
				printk("Message got lost \n");
				test_assert(0);
				continue;
			}
			
			//printk("DEBUG Flags: [enable | perval]: %i|%i\n", 
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_ENABLE),
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL));

			// need to clear flag
			irqtester_fe310_clear_all_valflags(dev);

			if(verbose>1){
				struct DrvValue_uint val;
				irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val);
				printkv(3, "Polled value (id: %i): %i updated at %u cyc \n", val._super.id_name, val.payload, val._super.time_cyc);
			}
		}

		if(use_valflag_plus_queue){
			int timeout = 10; // shouldn't occur
			u32_t start_cyc = get_cycle_32();
		
			irqtester_fe310_fire(dev);
			struct DrvEvent evt;			

			if(0 != k_msgq_get(&drv_q_rx, &evt, 100)){
				printk("Queue message got lost");
				test_assert(0);
				continue;
			}

			while(timeout > 0 && (0 == irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL))){
				timeout--; // high precision busy wait
			}
			delta_cyc = get_cycle_32() - start_cyc;

			if(timeout <= 0){
				test_assert(0);
				printk("Valflag message got lost \n");
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
				printkv(3, "Polled value (id: %i): %i updated at %u cyc \n", val._super.id_name, val.payload, val._super.time_cyc);
			}
		}

		// check value
		struct DrvValue_uint val_check;
		irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val_check);
		test_assert(val_check.payload == i);

		// catch timer overflow
		if(delta_cyc < 0){
			printk("Time overflow detected, discarding value");
			timing_res[i] = -1;
			continue; // ignores value
		}
		// save timing val
		timing_res[i] = delta_cyc;
		
	}

	test_warn_on_new_error();
	irqtester_fe310_reset_hw(dev);
	
}

static void irq_handler_clear_irq_2(void){
	irqtester_fe310_clear_1(dev_cp);
}

// test for a given delta_cyc (single value), whether interrupts can be cleared in time
// note: only for a single value, can't observe cache warmup
void test_irq_throughput_1(struct device * dev, int delta_cyc, int * status_res, int num_runs){

	dev_cp = dev; // store to static var to have access

	// configure an interrupt
	struct DrvValue_uint reg_num = {.payload=num_runs};
	struct DrvValue_uint reg_period = {.payload=delta_cyc};	
	struct DrvValue_uint status_1;

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);
	irqtester_fe310_register_callback(dev, IRQ_1, irq_handler_clear_irq_2);


	irqtester_fe310_fire_1(dev);
	u32_t t_sleep_ms = CYCLES_CYC_2_MS(num_runs * delta_cyc);
	if (t_sleep_ms == 0)
		t_sleep_ms  = 1;
	//printk("Sleeping for %i ms \n", t_sleep_ms);
	k_sleep(t_sleep_ms); // sleep in ms, run at 65 MHz

	// check whether status was incremented
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	
	*status_res = status_1.payload; 

	// note: curently status can't be cleared for next run

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
static int * _status_arr; // pointer to array
static int _status_arr_length;
static int _num_max_calls_handler;
static int i_handler;
static u32_t status_stamp;
#define IRQ1_CLEAR  0x201C
#define IRQ1_STATUS  0x2018
// if need to make global by "extern" in tests.h
/*
int * _status_arr = 0; // pointer to array
int _status_arr_length  = 0;
int _num_max_calls_handler = 0;
int i_handler = 0;
u32_t status_stamp;
*/
K_SEM_DEFINE(handler_sem, 0, 1);
static void irq_handler_clear_and_check(void){
	// attention: may overwrite beginning of status_arr
	// since handler is invoked even if handler_sem might already
	// be given	
	
	int i_arr = i_handler % _status_arr_length;

	if(i_handler < _status_arr_length){
		// read error status
		// SLOW
		//struct DrvValue_uint val;
		//irqtester_fe310_get_reg(dev_cp, VAL_IRQ_1_STATUS, &val);
		//u32_t status_new = val.payload;
		// Fast but ugly
		u32_t status_new = 0;
        reg_read_uint(IRQ1_STATUS, &status_new);

		// save how many irqs missed since handler called alst time
		_status_arr[i_arr] = status_new - status_stamp;
		// debug
		//printk("Saving %u errors in i_handler run %i \n", status_new - status_stamp, i_handler);
		status_stamp = status_new;
	}

	i_handler++;
	// using this driver function here is REALLY slow 1500 vs 200 cyc
	irqtester_fe310_clear_1(dev_cp);
	// faster, but evil to call from ISR
	//struct DrvValue_bool val;
	//val.payload = 1;
	//irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
	//val.payload = 0;
	//irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
	//reg_write_short(IRQ1_CLEAR, 1);
    //reg_write_short(IRQ1_CLEAR, 0);

	// let continue and give only once (high handler frequency)
	if(i_handler > _num_max_calls_handler && k_sem_count_get(&handler_sem) != 1)
		k_sem_give(&handler_sem);
}

// get number of missed handlers for every handler call
// to analyze warm up behaviour of icache 
// if num_runs > len status_arr gets overwritten in fifo manner
void test_irq_throughput_2(struct device * dev, int period_cyc, int num_runs, int status_arr[], int len){

	dev_cp = dev; // store to static var to have access
	_status_arr_length = len;
	_status_arr = status_arr;

	// reset if called mutliple times
	i_handler = 0;
	
	k_sem_init(&handler_sem, 0, 1);
	_num_max_calls_handler = num_runs;


	// configure an interrupt to fire 
	// attention: short periods in combination with high reg_num
	// can freeze whole rtos!
	int timeout = 5; //ms
	struct DrvValue_uint reg_num = {.payload= (2000*CYCLES_MS_2_CYC(timeout)) / period_cyc};
	struct DrvValue_uint reg_period = {.payload=period_cyc};	
	//struct DrvValue_uint status_1;

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);
	irqtester_fe310_register_callback(dev, IRQ_1, irq_handler_clear_and_check);

	irqtester_fe310_fire_1(dev);

	// semaphore is faster than sleeping, but unstable at high freqs
	k_sem_take(&handler_sem, timeout);

	
	//while(i_handler < num_runs && timeout > 0){
	//	k_sleep(1); 
		//printk("[%u] Waiting for handler \n", i_handler);
	//	timeout--;
	//}
	
	
	irqtester_fe310_reset_hw(dev);
	irqtester_fe310_unregister_callback(dev, IRQ_1);

	// print array
    //for(int i=0; i<num_runs; i++){
    //    printk("%i, ", _status_arr[i]);
    // }
    // printk("\n");

	if(timeout <= 0){
		// couldn't invoke handler, so we definetely missed clearing
		for(int i=i_handler; i<_status_arr_length; i++)
			_status_arr[i] = -1;
	}

}


void test_poll_throughput_1(struct device * dev, int period_cyc, int num_runs, int status_arr[], int len){

	u32_t period_poll = period_cyc;
	
	if(len != num_runs){
		printk("ERROR: num_runs should equal len of status_arr \n");
		return;
	}

	static int status_stamp = 0;

	// configure an interrupt for irq_1
	struct DrvValue_uint irq_num = {.payload=1};
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &irq_num);
    
	// configure polling dsp_3 block 
	struct DrvValue_uint delay = {.payload=period_poll};
	struct DrvValue_uint period = {.payload=period_poll};
	// configure huge # of fire events. We may miss many.
	struct DrvValue_uint num_rep = {.payload=100*num_runs}; // ~inf
	struct DrvValue_uint duty = {.payload=period_poll/2};

	irqtester_fe310_set_reg(dev, VAL_DSP_3_PERIOD, &period);
	irqtester_fe310_set_reg(dev, VAL_DSP_3_DUTY, &duty);
	irqtester_fe310_set_reg(dev, VAL_DSP_3_NUM_REP, &num_rep);
	// delay != 0 will activate polling device
	irqtester_fe310_set_reg(dev, VAL_DSP_3_DELAY, &delay);

	struct DrvValue_uint val;
	struct DrvValue_bool ready_val;

	irqtester_fe310_fire_1(dev);
	u32_t i_poll = 0;
	bool ready = false;
	u32_t counter_reg = 0;
	int timeout_nextready = period_poll;

	while(i_poll < num_runs){

		timeout_nextready = period_poll;
		ready = false;

		while(!ready && timeout_nextready >= 0){		
			
			irqtester_fe310_get_reg(dev, VAL_DSP_3_READY, &ready_val);
			ready = ready_val.payload;

			timeout_nextready--;
		}
	
		if(timeout_nextready > 0){
			// value successfully polled
			irqtester_fe310_get_reg(dev, VAL_DSP_3_ID_COUNTER, &val);
			counter_reg = val.payload;
			// clear current id
			val.payload = counter_reg;
			irqtester_fe310_set_reg(dev, VAL_DSP_3_CLEAR_ID, &val);

			// save how many times missed
			irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &val);
			int status_new = val.payload;

			//printk("Debug. Status_3 new: %i, stamp: %i\n", status_new, status_stamp);

			status_arr[i_poll] = status_new - status_stamp;
			status_stamp = status_new;
			
		}
		else{
			status_arr[i_poll] = -1;
		}

		i_poll++;
		
	}


	// need to clean up input regs
	//struct DrvValue_bool reset = {.payload=1};
	// don't reset status counter
	//irqtester_fe310_set_reg(dev, VAL_DSP_3_RESET, &reset);
	//reset.payload = 0;
	//irqtester_fe310_set_reg(dev, VAL_DSP_3_RESET, &reset);
	
	// stop running
	val.payload = 0;
	irqtester_fe310_set_reg(dev, VAL_DSP_3_NUM_REP, &val);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &val);
	// avoid clearing of non zero id at next usage
	irqtester_fe310_set_reg(dev, VAL_DSP_3_CLEAR_ID, &val);

	// delay = 0 will deactivate polling device
	delay.payload = 0;
	irqtester_fe310_set_reg(dev, VAL_DSP_3_DELAY, &delay);

}

// @brief: 	Fire irq to run through SM. Use together with an action handler
//			that saves the accumulated sum of state_ids in VAL_IRQ_0_PERVAL
void test_state_mng_1(struct device * dev, int sum_expect){
	printkv(3, "DEBUG: Entering test_state_mng_1 \n");
	// fire irq1, should trigger a single run through complete SM
    struct DrvValue_uint reg_num = {.payload=1};
	struct DrvValue_uint reg_period = {.payload=200};
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	// even for only 1 repetition, never set period <= 1
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);
	irqtester_fe310_fire_1(dev);
	struct DrvValue_uint res_perval = {.payload=0};;

	// after run through, sm will be in idle state, 
	// ready to give back control to threads of higher prio

	int timeout = 10; // ms
	while(res_perval.payload != sum_expect && timeout > 0){
		// hand off to state_manager thread
		// wait instead yield, since prio of this thread is higher than state_manager
		k_sleep(1);
		timeout--;
		// after firing, this thread should gain control again when 
		// state machine returns to IDLE
		// registered action writes state_sum to perval
		irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &res_perval);
		//printkv(3, "DEBUG: Waiting for SM reset. Timeout %i, SM alive %i, state %i, state_sum %i \n", 
		//	  timeout, state_mng_is_running(), state_mng_get_current(), res_perval.payload);
		
	}
	
	// if called from test_state_mng_2, perval is set in registered sm action
	if(res_perval.payload != sum_expect){
		test_assert(res_perval.payload == sum_expect);
		printkv(1, "WARNING: test_state_mng_1 assert failed. timeout %i, perval %i \n",
				timeout, res_perval.payload);
	}
	printkv(3, "DEBUG: Finshed SM cycle. SM alive %i, state %i, state_sum %i \n", 
			  state_mng_is_running(), state_mng_get_current(), res_perval.payload);
	
	irqtester_fe310_reset_hw(dev);
	
	test_warn_on_new_error();
	
}

#include <uart.h>
// tests whether last received char == test_ch
int test_uart_print_rx_buf(struct device * dev, char * test_ch){
	unsigned char char_in;
	char * name = dev->config->name;
	if(0 == uart_poll_in(dev, &char_in)){
		
		printkv(2, "%s received: %c", name, char_in);
		while(0 == uart_poll_in(dev, &char_in)){
			printkv(2, "%c", char_in);
		}
		printkv(2, "\n");
	}
	if(*test_ch == char_in)
		return 0;
	//printk("%s failed\n", name);
	return 1;
	
}

// send chars over uart and receive afterwards
// connect uart0 tx (pin J58 PMOD1_1) with uart0/1 rx (pin PMOD1_0 / pin PMOD1_2)
void test_uart_1(struct device * dev0, struct device * dev1){
	int j=48;  // ASCII: 0
	printkv(2, "UART0 Poll out test: \n");

	// slow, nice output for verbosity > 2
	int t_wait_ms = 1;
	int n_runs = 2;
	if(print_get_verbosity() > 2){
		t_wait_ms = 200;
		n_runs = 50;
	}

	// save error count
	int err_save = test_get_err_count();

	for(int n=0; n<n_runs; n++){

		unsigned char write_char = j;
		
		if(write_char + j < 126){	// last printable char in ASCII
			// repeat char
			for(int i=0; i<10; i++){
				uart_poll_out(dev0, write_char);
				k_sleep(t_wait_ms);
				printkv(2, "%c", write_char);
			}
			printkv(2, "\n");
			int res = 0;
			res += test_uart_print_rx_buf(dev0, &write_char);	// print all rx
			res += test_uart_print_rx_buf(dev1, &write_char);	// print all rx
			if(res > 1)
				test_assert(0);	// > 1 uart rx have failed
			
			j++;
		}
		else
			j = 32; // first printable char in ASCII
	}

	// short output if verb = 1
	printkv(1, "UART0 Poll out test... "); 
	printkv(1, (test_get_err_count() == err_save ? "ok\n" : "failed\n"));
}




int find_min_in_arr(int arr[], int len, int * pos){
	if(len < 1){
		*pos = -1;
		return -1;
	} 
	if(pos == NULL){	// catch zero dereference
		int a = 0;	// dummy
		pos = &a;
	}
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
	if(len < 1){
		*pos = -1;
		return -1;
	} 
	if(pos == NULL){	// catch zero dereference
		int a = 0;	// dummy
		pos = &a;
	}
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
	
	int delta_min = find_min_in_arr(timing, len, NULL);
	int delta_max = find_max_in_arr(timing, len, NULL);
	if(delta_max > global_max_cyc)
		global_max_cyc = delta_max;
	int delta_avg = calc_avg_arr(timing, len, true); // ignore timer overflow vals

	printkv(verbosity, "Reaction out of %i runs in cpu cycles [avg/min/max]: %i/%i/%i \n",
		 len, delta_avg, delta_min, delta_max);
	printkv(verbosity, "Reaction out of %i runs in us [avg/min/max]: %i/%i/%i \n",
		 len, CYCLES_CYC_2_US(delta_avg), 
		 CYCLES_CYC_2_US(delta_min),
		 CYCLES_CYC_2_US(delta_max));

	printkv(verbosity, "Detailed reaction in cycles: \n {[");
	for(int i=0; i < len; i++){
		printkv(verbosity, "%i, ", timing[i]);
	}
	printkv(verbosity, "]} \n");

}

void print_continous(int timing[], int len, int verbosity){
	/*
	int delta_min = find_min_in_arr(timing, len, 0);
	int delta_max = find_max_in_arr(timing, len, 0);
	if(delta_max > global_max_cyc)
		global_max_cyc = delta_max;
	int delta_avg = calc_avg_arr(timing, len, true); // ignore timer overflow vals
	*/
	if(verbosity > 0){
		printk("{[");
		for(int i=0; i < len; i++){
			printk("%i, ", timing[i]);
		}
		printk("]} \n");
	}
}



#endif