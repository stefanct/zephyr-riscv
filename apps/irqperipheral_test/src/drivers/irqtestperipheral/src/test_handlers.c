
// uses direct getters
void _irq_0_handler_1(void){

	// todo: for new hardware revision: signal irq cleared to hw

	/* generic part */
	struct device * dev = DEV();
	if(dev == NULL)
		return;	// safety first

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	u32_t perval_0;
	bool enable;
	irqtester_fe310_get_perval(dev, &perval_0);
	irqtester_fe310_get_enable(dev, &enable);
	

	// write into internal data pools 
	u32_t now_cyc = get_cycle_32();
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval_0;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_cyc = now_cyc; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_cyc = now_cyc;

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=VAL_T_INT, .event_type=EVT_T_VAL_UPDATE};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=VAL_T_BOOL, .event_type=EVT_T_VAL_UPDATE};

	// inefficient to test bit here, but this is a default ISR for testing only
	if(test_any_send_flag(dev)){
		send_event_rx(dev, &evt_perval);
		send_event_rx(dev, &evt_enable);
	}
	if(test_flag(dev, IRQT_VALFLAGS_RX_ENABLED)){
		flag_event_rx(dev, &evt_perval);
		flag_event_rx(dev, &evt_enable);	
	}

}

// uses generic getters
// often arround 15 cycles slower than direct getters (84 vs 67)
// sometimes also faster (cache influenece?)
void _irq_0_handler_2(void){

	// todo: for new hardware revision: signal irq cleared to hw

	/* generic part */
	struct device * dev = DEV();
	if(dev == NULL)
		return;	// safety first

	#ifndef TEST_MINIMAL

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	struct DrvValue_uint perval_0 = {.payload = 0};
	struct DrvValue_bool enable= {.payload = 0};
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &perval_0);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	

	// write into internal data pools 
	u32_t now_cyc = get_cycle_32();
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval_0.payload;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_cyc = now_cyc; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable.payload;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_cyc = now_cyc;

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=VAL_T_INT, .event_type=EVT_T_VAL_UPDATE};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=VAL_T_BOOL, .event_type=EVT_T_VAL_UPDATE};

	// inefficient to test bit here, but this is a default ISR for testing only
	if(test_any_send_flag(dev)){
		send_event_rx(dev, &evt_perval);
		send_event_rx(dev, &evt_enable);
	}
	if(test_flag(dev, IRQT_VALFLAGS_RX_ENABLED)){
		flag_event_rx(dev, &evt_perval);
		flag_event_rx(dev, &evt_enable);	
	}
	#endif
}

// don't copy into memory pools, just notify
void _irq_0_handler_3(void){

	// todo: for new hardware revision: signal irq cleared to hw

	/* generic part */
	struct device * dev = DEV();
	if(dev == NULL)
		return;	// safety first

	#ifndef TEST_MINIMAL 
	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	struct DrvValue_uint perval_0;
	struct DrvValue_bool enable;
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &perval_0);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	

	/* write into internal data pools */
	//u32_t now_cyc = get_cycle_32(); // might be written into event
	/*
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval_0.payload;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_cyc = now_cyc; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable.payload;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_cyc = now_cyc;
	*/

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=VAL_T_INT, .event_type=EVT_T_VAL_UPDATE};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=VAL_T_BOOL, .event_type=EVT_T_VAL_UPDATE};

	// inefficient to test bit here, but this is a default ISR for testing only
	if(test_any_send_flag(dev)){
		send_event_rx(dev, &evt_perval);
		send_event_rx(dev, &evt_enable);
	}
	if(test_flag(dev, IRQT_VALFLAGS_RX_ENABLED)){
		flag_event_rx(dev, &evt_perval);
		flag_event_rx(dev, &evt_enable);	
	}
	#endif
}

// minimal handler, just set flag
void _irq_0_handler_4(void){
	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);
}


// hand optimized handler, flag + queue
void _irq_0_handler_5(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	int perval_0;
	irqtester_fe310_get_perval(dev, &perval_0);
	
	// write into internal data pools 
	// no timing support
	_values_uint[VAL_IRQ_0_PERVAL - 1].payload = perval_0; // only works for uint type values
	struct DrvEvent evt_irq0   = {.val_id=_NIL_VAL, .event_type=EVT_T_IRQ, .irq_id=IRQ_0};

	// manually inlining send, flag functions
	k_msgq_put(data->_queue_rx, &evt_irq0, K_NO_WAIT);
	atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);
}
// hand optimized handler irq0, flag only
void _irq_0_handler_6(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	int perval_0;
	irqtester_fe310_get_perval(dev, &perval_0);
	
	// write into internal data pools 
	// no timing support
	_values_uint[VAL_IRQ_0_PERVAL - 1].payload = perval_0; // only works for uint type values

	// manually inlining send, flag functions
	atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);
}

// hand optimized handler irq1, queue 
void _irq_1_handler_0(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	struct DrvEvent evt_irq1   = {.val_id=_NIL_VAL, .event_type=EVT_T_IRQ, .irq_id=IRQ_1, .prio=1};

	// manually inlining send, flag functions
	//if(0 != k_msgq_put(data->_queue_rx, &evt_irq1, K_NO_WAIT))
	//	SYS_LOG_WRN("Couln't send event irq1 to driver queue");
	k_msgq_put(data->_queue_rx, &evt_irq1, K_NO_WAIT);
	//SYS_LOG_WRN("IRQ1");

	// clear irq hardware on hw rev 2
	irqtester_fe310_clear_1(dev);

	/* dbg
	   u32_t reg = 5;
		__asm__ volatile("csrr t0, mtvec"); 
		__asm__ volatile("csrw mtvec, %0" :: "r" (reg)); 
		__asm__ volatile("csrw mtvec, t0"); 
	*/
}

// hand optimized handler irq1, queue, no clear
void _irq_1_handler_1(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	struct DrvEvent evt_irq1   = {.val_id=_NIL_VAL, .event_type=EVT_T_IRQ, .irq_id=IRQ_1};

	// manually inlining send, flag functions
	k_msgq_put(data->_queue_rx, &evt_irq1, K_NO_WAIT);

}

// hand optimized handler irq2, valflag and load FAKE value
void _irq_2_handler_0(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	
	// just to simulate timing
	//int perval_0;
	//irqtester_fe310_get_perval(dev, &perval_0);
	
	// fake value with static counter
	static u32_t count_irq2;
	count_irq2++;
	_values_uint[VAL_IRQ_0_PERVAL - 1].payload = count_irq2; // only works for uint type values
	
	// logging: slow
	//if(!atomic_test_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL))
	//	LOG_PERF("[%u] Set perval valflag", get_cycle_32());

	// manually inlining send, flag functions
	atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);

	irqtester_fe310_clear_2(dev);
}

void _irq_2_handler_1(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */

	// manually inlining send, flag functions
	atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);

	irqtester_fe310_clear_2(dev);
}
