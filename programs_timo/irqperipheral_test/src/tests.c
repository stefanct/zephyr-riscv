
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

K_SEM_DEFINE(wait_sem, 0, 1);
static u32_t time_isr;
static struct device * dev_cp;

void isr_time(void){
	int res_perval;
	bool res_enable;
	irqtester_fe310_get_perval(dev_cp, &res_perval);
	irqtester_fe310_get_enable(dev_cp, &res_enable);

	time_isr =  k_cycle_get_32();
	k_sem_give(&wait_sem);
	printk("Callback fired at %i ticks. [perval / enable]: %i, %i \n", time_isr, 
		res_perval, res_enable);
}

void test_interrupt_timing(struct device * dev, int num_runs){

	dev_cp = dev; // store to static var to have access

	irqtester_fe310_register_callback(dev, &isr_time);

	// to measure reaction time in ticks
	int delta_min = 0;
	int delta_max = 0;
	int delta_avg = 0;
	int delta_cyc = 0;
	delta_avg = getAvg(0, true); // init static vars of average helper

	for(int i=0; i<num_runs; i++){

		u32_t start_cyc = k_cycle_get_32();
		irqtester_fe310_set_value(dev, i);
		irqtester_fe310_fire(dev);

		// measure time since fire
		// do only for first message, because handling the first
		// messages takes some time - and we don't want to measure this
		if (k_sem_take(&wait_sem, 500) != 0) {
        	printk("Warning: No interrupt detected");
    	} else {

			delta_cyc = time_isr - start_cyc;
			
			// catch timer overflow
			if(delta_cyc < 0)
				delta_cyc = delta_avg; // ~ignores value
			if(delta_cyc < delta_min || delta_min == 0)
				delta_min = delta_cyc;
			if(delta_cyc > delta_max)
				delta_max = delta_cyc;
			delta_avg = getAvg(delta_cyc, false);
		}

	}

	
	printk("Reaction out of %i runs in cycles [avg/min/max]: %i/%i/%i \n",
		 num_runs, delta_avg, delta_min, delta_max);
	printk("Reaction out of %i runs in ns [avg/min/max]: %i/%i/%i \n",
		 num_runs, SYS_CLOCK_HW_CYCLES_TO_NS(delta_avg), 
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_min),
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_max));
	
}

/*
 * Warning: test only with one interrupt fired.
 * Performance will deteriorate if flooded with interrupts and events.
 */
K_MSGQ_DEFINE(drv_q_rx, sizeof(struct DrvEvent), 10, 4);
void test_queue_rx_timing(struct device * dev, int num_runs){

   	// create and register with driver queue
	struct DrvEvent evt;

	irqtester_fe310_register_queue_rx(dev, &drv_q_rx);
	irqtester_fe310_enable_queue_rx(dev);

	// to measure reaction time in ticks
	int delta_min = 0;
	int delta_max = 0;
	int delta_avg = 0;
	int delta_cyc = 0;
	delta_avg = getAvg(0, true); // init static vars of average helper

	for(int i=0; i<num_runs; i++){
		irqtester_fe310_set_value(dev, i);
		u32_t start_cyc = k_cycle_get_32();
		irqtester_fe310_fire(dev);
		
		// wait for new queue element
		int num_msgs = k_msgq_num_used_get(&drv_q_rx);
		for(int i=0; i<num_msgs; i++){			
			if(0 != k_msgq_get(&drv_q_rx, &evt, K_NO_WAIT))
				printk("Message got lost");
			else{
				// measure time since fire
				// do only for first message, because handling the first
				// messages takes some time - and we don't want to measure this
				if(i==0){
					delta_cyc = k_cycle_get_32() - start_cyc;
					
					// catch timer overflow
					if(delta_cyc < 0)
						delta_cyc = delta_avg; // ~ignores value
					if(delta_cyc < delta_min || delta_min == 0)
						delta_min = delta_cyc;
					if(delta_cyc > delta_max)
						delta_max = delta_cyc;
					delta_avg = getAvg(delta_cyc, false);
				}

				irqtester_fe310_dbgprint_event(dev, &evt);
			}
		}
	}

	
	printk("Reaction out of %i runs in cycles [avg/min/max]: %i/%i/%i \n",
		 num_runs, delta_avg, delta_min, delta_max);
	printk("Reaction out of %i runs in ns [avg/min/max]: %i/%i/%i \n",
		 num_runs, SYS_CLOCK_HW_CYCLES_TO_NS(delta_avg), 
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_min),
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_max));
	
}


