
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

void irq_handler_mes_time(void){
	int res_perval;
	bool res_enable;
	irqtester_fe310_get_perval(dev_cp, &res_perval);
	irqtester_fe310_get_enable(dev_cp, &res_enable);

	time_isr =  k_cycle_get_32();
	k_sem_give(&wait_sem);
	//printk("Callback fired at %i ticks. [perval / enable]: %i, %i \n", time_isr, 
	//	res_perval, res_enable);
}

void test_interrupt_timing(struct device * dev, int num_runs, int verbose){

	dev_cp = dev; // store to static var to have access

	printk("Make sure that non-default irq handler is installed to driver! \n");

	// to measure reaction time in ticks
	int delta_min = 0;
	int delta_max = 0;
	int delta_avg = 0;
	int delta_cyc = 0;
	delta_avg = getAvg(0, true); // init static vars of average helper

	if(verbose>0)
		printk("Detailed timing in cycles: \n");
	for(int i=0; i<num_runs; i++){

		u32_t start_cyc = k_cycle_get_32();
		irqtester_fe310_set_value(dev, i);
		irqtester_fe310_fire(dev);

		// measure time since fire
		// do only for first message, because handling the first
		// messages takes some time - and we don't want to measure this
		if (k_sem_take(&wait_sem, 100) != 0) {
        	printk("Warning: Failed to take semaphore \n");
    	} else {

			delta_cyc = time_isr - start_cyc;
			
			// catch timer overflow
			if(delta_cyc < 0){
				printk("Time overflow detected, discarding value");
				continue; // ignores value
			}
			if(delta_cyc < delta_min || delta_min == 0)
				delta_min = delta_cyc;
			if(delta_cyc > delta_max)
				delta_max = delta_cyc;
			delta_avg = getAvg(delta_cyc, false);
			if(verbose>0)
				printk("%i, ", delta_cyc);
		}

	}
	if(verbose>0)
		printk("\n");
	
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
K_FIFO_DEFINE(drv_fifo_rx);
K_SEM_DEFINE(drv_sem_rx, 0, 10);
struct DrvEvent drv_evt_arr_rx[10];
void test_rx_timing(struct device * dev, int num_runs, int mode, int verbose){

   	
	bool use_queue = false;
	bool use_fifo = false;
	bool use_semArr = false;
	bool use_valflag = false;
	switch(mode){
		case 0: use_queue = true;
			break;
		case 1: use_fifo = true;
			break;
		case 2: use_semArr = true;
			break;
		case 3: use_valflag = true;
			break;
		default: printk("Error: unknown mode %i", mode);
			return;
	}
	
	printk("DEBUG: queue: %i, fifo: %i, semArr:%i , valflag: %i \n",use_queue, use_fifo, use_semArr, use_valflag);
	if(use_valflag + use_queue + use_fifo + use_semArr != 1)
		return;

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
	// to measure reaction time in hw cycles
	int delta_min = 0;
	int delta_max = 0;
	int delta_avg = 0;
	int delta_cyc = 0;
	delta_avg = getAvg(0, true); // init static vars of average helper

	if(verbose>0)
		printk("Detailed timing in cycles: \n");

	for(int i=0; i<num_runs; i++){
		irqtester_fe310_set_value(dev, i);

		if(use_fifo){
			u32_t start_cyc = k_cycle_get_32();
			irqtester_fe310_fire(dev);

			struct DrvEvent evt; // empty container
			struct DrvEvent * p_evt;
			p_evt = k_fifo_get(&drv_fifo_rx, 100);
			evt = *p_evt;
			if(NULL == p_evt){
				printk("Message got lost");
				continue;
			}
			delta_cyc = k_cycle_get_32() - start_cyc;

			if(verbose>1)
				irqtester_fe310_dbgprint_event(dev, &evt);

			// purge fifo, since only interessted in reaction till first msg
			while(NULL != k_fifo_get(&drv_fifo_rx, K_NO_WAIT)){
				; // nothing to do here
			}

		}
		if(use_semArr){
			u32_t start_cyc = k_cycle_get_32();
			irqtester_fe310_fire(dev);

			struct DrvEvent evt;
			
			// short version, only first msg
			if(0 != irqtester_fe310_receive_evt_from_arr(dev, &evt, K_MSEC(100)) ){
				printk("Message got lost");
				continue;
			}
			delta_cyc = k_cycle_get_32() - start_cyc;
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
					delta_cyc = k_cycle_get_32() - start_cyc;

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
			u32_t start_cyc = k_cycle_get_32();
			irqtester_fe310_fire(dev);
			
			struct DrvEvent evt;			
			// short version, just measure delay till first message received
			if(0 != k_msgq_get(&drv_q_rx, &evt, 100)){
				printk("Message got lost");
				continue;
			}
			delta_cyc = k_cycle_get_32() - start_cyc;
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
						delta_cyc = k_cycle_get_32() - start_cyc;
					}
					//irqtester_fe310_dbgprint_event(dev, &evt);
				}
			}
			*/
		}

		if(use_valflag){
			// not event driven
			// just checks whether a specified value was loaded into driver mem
			u32_t start_cyc = k_cycle_get_32();
			int timeout = 10; // shouldn't occur

			//printk("DEBUG Flags: [enable | perval]: %i|%i\n", 
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_ENABLE),
			//	irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL));

			irqtester_fe310_fire(dev);

			while(timeout > 0 && (0 == irqtester_fe310_test_valflag(dev, VAL_IRQ_0_PERVAL))){
				timeout--; // high precision busy wait
			}
			delta_cyc = k_cycle_get_32() - start_cyc;
			if(timeout <= 0){
				printk("Message got lost \n");
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
				printk("Polled value (id: %i): %i updated at %u ns \n", val.base.id_name, val.payload, val.base.time_ns);
			}
		}

		// catch timer overflow
		if(delta_cyc < 0){
			printk("Time overflow detected, discarding value");
			continue; // ignores value
		}
		if(delta_cyc < delta_min || delta_min == 0)
			delta_min = delta_cyc;
		if(delta_cyc > delta_max)
			delta_max = delta_cyc;
		delta_avg = getAvg(delta_cyc, false);
		if(verbose>0)
			printk("%i, ", delta_cyc);
		
	}
	if(verbose>0)
		printk("\n");

	
	printk("Reaction out of %i runs in cycles [avg/min/max]: %i/%i/%i \n",
		 num_runs, delta_avg, delta_min, delta_max);
	printk("Reaction out of %i runs in ns [avg/min/max]: %i/%i/%i \n",
		 num_runs, SYS_CLOCK_HW_CYCLES_TO_NS(delta_avg), 
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_min),
		 SYS_CLOCK_HW_CYCLES_TO_NS(delta_max));
	
}


