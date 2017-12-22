#ifndef TEST_MINIMAL

#include "../utils.h"
#include "../irqtestperipheral.h"
#include "../state_manager.h"
#include "tests.h"
#include "cycles.h"

// ugly globals
int error_count = 0;
int error_stamp = 0;

void test_assert(bool expression){
	if(!expression)
		error_count++;
}

static void warn_on_new_error(void){
	if(error_count > error_stamp){
		printk("WARNING: Test failed with %i errors. Total now %i \n", error_count - error_stamp, error_count);
		error_stamp = error_count;
	}
}

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

// tests functionality of hw rev 1
// compatible with hw revs: 1,2,3
void test_hw_rev_1_basic_1(struct device * dev){
	// test generic reg setters and getters, driver mem getter
	struct DrvValue_uint val = {.payload=42}; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val);
	struct DrvValue_uint test;

	// without load to driver, value should be 0 
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_reg perval0: %i \n", test.payload);
	test_assert(test.payload == (u32_t)0);

	// firing loads register into driver values
	irqtester_fe310_fire(dev);
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_val perval0: %i \n", test.payload);
	test_assert(test.payload == (u32_t)42);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_STATUS, &test);
	printk("get_reg status0: %i \n", test.payload);
	test_assert(test.payload == (u32_t)0); // no error code status

	// disabling won't fire the interrupt, so nothing loaded
	struct DrvValue_bool enable = {.payload=false}; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	struct DrvValue_uint val2 = {.payload=7}; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_VALUE, &val2);
	irqtester_fe310_fire(dev);
	// load from memory value
	irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &test);
	printk("get_val perval0: %i \n", test.payload);
	test_assert(test.payload == (u32_t)42);
	
	// load from register should still work after re-enabling
	enable.payload=true; 
	irqtester_fe310_set_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &test);
	test_assert(test.payload == (u32_t)7);
	printk("get_reg perval0: %i \n", test.payload);

	printk("Trying to get invalid value. Expect and ignore error. \n");
	int ret = irqtester_fe310_get_val((u32_t)-1, &test);
	test_assert(ret != 0);
	
	
}

static struct device * dev_cp;
void irq_handler_clear_irq_1(void){
	struct DrvValue_bool val;
	val.payload = 1;
	irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
	u32_t t = get_cycle_32();
	printk("Interrupt: Cleared at %i cycles \n", t);
	// reset clear input
	val.payload = 0;
	irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_CLEAR, &val);
}

// test new DPS blocks 
// compatible with hw revs: 2
void test_hw_rev_2_basic_1(struct device * dev){

	u32_t NUM_FIRE_1 = 3;
	u32_t PERIOD_FIRE_1 = 1000000;  // clock cycles
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
	printk("get_reg status_1: %i \n", status_1.payload);
	printk("get_reg status_2: %i \n", status_2.payload);
	
	// fire irq1 3x, with handler thath won't clear interrupt
	irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_1);
	irqtester_fe310_register_callback(dev, IRQ_2, _irq_1_handler_1);
	irqtester_fe310_fire_1(dev);
	irqtester_fe310_fire_2(dev);


	u32_t t_sleep_ms = (NUM_FIRE_1 * PERIOD_FIRE_1) / 65000;
	printk("Sleeping for %i ms \n", t_sleep_ms);
	k_sleep(t_sleep_ms); /// sleep in ms, run at 65 MHz
	
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_NUM_REP, &val);
	printk("get_reg num_rep_1: %i \n", val.payload);
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_PERIOD, &val);
	printk("get_reg period_1: %i \n", val.payload);
	
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("get_reg status_1: %i \n", status_1.payload);
	irqtester_fe310_get_reg(dev, VAL_IRQ_2_STATUS, &status_2);
	printk("get_reg status_2: %i \n", status_2.payload);

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
	k_sleep(NUM_FIRE_1 * PERIOD_FIRE_1 / 65000); 
	printk("Fired at %i cycles \n", t);

	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("get_reg status_1: %i \n", status_1.payload);
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_CLEAR, &clear_1);
	printk("get_reg clear_1: %i \n", clear_1.payload);

	// we did clear the interrupt, so status should be unchanged
	test_assert(status_1.payload == NUM_FIRE_1);

	// reset to default handlers
	irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);
	irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);

}




K_SEM_DEFINE(wait_sem, 0, 1);
static u32_t isr_perval;
static u32_t time_isr;
static struct device * dev_cp;

void irq_handler_mes_time(void){
	u32_t res_perval;
	bool res_enable;
	irqtester_fe310_get_perval(dev_cp, &res_perval);
	irqtester_fe310_get_enable(dev_cp, &res_enable);
	isr_perval = res_perval;

	time_isr =  get_cycle_32();
	k_sem_give(&wait_sem);
	//printk("Callback fired at %i ticks. [perval / enable]: %i, %i \n", time_isr, 
	//	res_perval, res_enable);
}

// warning, can't test whether value in ISR is correct
void test_interrupt_timing(struct device * dev, int timing_res[], int num_runs, int verbose){

	dev_cp = dev; // store to static var to have access

	printk("Make sure that non-default irq handler is installed to driver! \n");

	u32_t delta_cyc;

	for(u32_t i=0; i<num_runs; i++){

		u32_t start_cyc = get_cycle_32();
		irqtester_fe310_set_value(dev, i);
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

	warn_on_new_error();

}

static void irq_handler_clear_irq_2(void){
	irqtester_fe310_clear_1(dev_cp);
}
// test for a given delta_cyc, whether interrupts can be cleared in time
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
	u32_t t_sleep_ms = (num_runs * delta_cyc) / 65000;
	if (t_sleep_ms == 0)
		t_sleep_ms  = 1;
	//printk("Sleeping for %i ms \n", t_sleep_ms);
	k_sleep(t_sleep_ms); // sleep in ms, run at 65 MHz

	// check whether status was incremented
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	
	*status_res = status_1.payload; 

	// note: curently status can't be cleared for next run

}

static int * _status_arr; // pointer to array
static int _status_arr_length;
static int _num_max_calls_handler;
static int i_handler;
static void irq_handler_clear_and_check(void){
	
	static u32_t status_stamp;
	int i_arr = i_handler % _status_arr_length;

	// read error status
	struct DrvValue_uint val;
	irqtester_fe310_get_reg(dev_cp, VAL_IRQ_1_STATUS, &val);
	u32_t status_new = val.payload;


	// save how many irqs missed since handler called alst time
	_status_arr[i_arr] = status_new - status_stamp;
	// debug
	//printk("Saving %u errors in i_handler run %i \n", status_new - status_stamp, i_handler);
	status_stamp = status_new;
	
	i_handler++;

	irqtester_fe310_clear_1(dev_cp);
}

// get number of missed handlers for every handler call
// to analyze warmp up behaviour of icache 
// if num_runs > len status_arr gets overwritten in fifo manner
void test_irq_throughput_2(struct device * dev, int period_cyc, int num_runs, int status_arr[], int len){

	dev_cp = dev; // store to static var to have access
	_status_arr_length = len;
	_status_arr = status_arr;

	// reset if called mutliple times
	i_handler = 0;
	_num_max_calls_handler = num_runs;


	// configure an interrupt to fire 
	// attention: short periods in combination with high reg_num
	// can freeze whole rtos!
	int timeout = 100; //ms
	struct DrvValue_uint reg_num = {.payload= (65000 * timeout) / period_cyc};
	struct DrvValue_uint reg_period = {.payload=period_cyc};	
	struct DrvValue_uint status_1;

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);
	irqtester_fe310_register_callback(dev, IRQ_1, irq_handler_clear_and_check);

	irqtester_fe310_fire_1(dev);

	// will at least fire (UINT32_MAX) until i_handler counted to num_runs
	
	while(i_handler < num_runs && timeout > 0){
		k_sleep(1); 
		timeout--;
	}
	
	reg_num.payload=0;
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_unregister_callback(dev, IRQ_1);

	if(timeout <= 0){
		// couldn't invoke handler, so we definetely missed clearing
		for(int i=i_handler; i<_status_arr_length; i++)
			_status_arr[i] = -1;
	}

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
	
	printk("DEBUG: queue: %i, fifo: %i, semArr:%i , valflag: %i, valflag plus queue: %i \n",
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

	u32_t delta_cyc;

	for(u32_t i=0; i<num_runs; i++){
		irqtester_fe310_set_value(dev, i);

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
				if(verbose>1){
					printk("Discarding fifo element %p. Event [cleared, val_id, val_type, event_type, irq_id]: %i, %i, %i, %i, %i \n" \
						,evt._reserved, evt.cleared, evt.val_id, evt.val_type, evt.event_type, evt.irq_id);
				}
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
				printk("Polled value (id: %i): %i updated at %u ns \n", val._super.id_name, val.payload, val._super.time_ns);
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
				printk("Polled value (id: %i): %i updated at %u ns \n", val._super.id_name, val.payload, val._super.time_ns);
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

	warn_on_new_error();
	
}

void test_uint_overflow(){
	print_dash_line();
	printk_framed("Now checking uint (timer) overrroll behaviour ");
    print_dash_line();

	printk("1: (non overroll) UINT_MAX - (UINT_MAX-1) \n2: (overroll) UINT_MAX + 10 - (UINT_MAX-1) \n");
	printk("Expect (1) + 10 == (2) \n");
	u32_t high_normal = UINT_MAX;
	u32_t high_ov = UINT_MAX + 10;
	u32_t low = UINT_MAX -1 ;

	u32_t dif_normal = high_normal - low;
	u32_t dif_ov = high_ov - low;

	test_assert(dif_normal + 10 == dif_ov );
	printk("dif normal: %u, overroll: %u \n", dif_normal, dif_ov);
	print_report(error_count);
}

void test_state_mng_1(struct device * dev){

	// calc expected state_sum for automatic config states.c
	int sum_expect = 0;
	for(int j=0; j<_NUM_CYCLE_STATES; j++){
		sum_expect += j;
	}
	irqtester_fe310_fire(dev);
	// have to wait, if prio of this thread is higher than state_manager
	k_yield();

	// after firing, this thread should gain control again when 
	// state machine returns to IDLE
	// registered action writes state_sum to perval
	struct DrvValue_uint res_perval;
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_VALUE, &res_perval);
	test_assert(res_perval.payload == sum_expect);
	printk("DEBUG: Finshed state machine cycle. State_sum %i \n", res_perval.payload);

	
	warn_on_new_error();
	
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

	printk("Reaction out of %i runs in cycles [avg/min/max]: %i/%i/%i \n",
		 len, delta_avg, delta_min, delta_max);
	printk("Reaction out of %i runs in ns [avg/min/max]: %i/%i/%i \n",
		 len, SYS_CLOCK_HW_CYCLES_TO_NS(delta_avg), 
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_min),
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_max));
	if(verbosity > 0){
		printk("Detailed reaction in cycles: \n {[");
		for(int i=0; i < len; i++){
			printk("%i, ", timing[i]);
		}
		printk("]} \n");
	}
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