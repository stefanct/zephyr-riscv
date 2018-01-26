/** 
 *  @file
 *  Driver for IrqTestPeripheral hw revision 1.
 *  Singleton, only register one instance with kernel!
 *  Based on gpio_fe310 driver by Jean-Paul Etienne <fractalclone@gmail.com>
 */

// todo: 
// - public functions should probably have a device param to differentiate between instances
// - decide whether pure singleton or move static vars to data
// - driver verbosity for debugging, implement via define, for reduced footprint

#include "irqtestperipheral.h"
#include <soc.h>
#include <atomic.h>

#define SYS_LOG_DOMAIN "IrqTestPeripheral"  
#ifdef TEST_MINIMAL
#warning "Building minimal version of driver"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_ERROR
#else
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <misc/printk.h> // debug only, 
#endif
#include <logging/sys_log.h>
#include "log_perf.h"
#include "cycles.h"

#define FE310_IRQTESTER_0_IRQ_0_PIN					53
#define FE310_IRQTESTER_0_IRQ_0             		(RISCV_MAX_GENERIC_IRQ + FE310_IRQTESTER_0_IRQ_0_PIN)
#define FE310_IRQTESTER_0_IRQ_1             		(RISCV_MAX_GENERIC_IRQ + FE310_IRQTESTER_0_IRQ_0_PIN + 1)
#define FE310_IRQTESTER_0_IRQ_2             		(RISCV_MAX_GENERIC_IRQ + FE310_IRQTESTER_0_IRQ_0_PIN + 2)
#define FE310_IRQTESTER_0_DSP_0_BASE_ADDR			0x2000
#define FE310_IRQTESTER_0_DSP_1_BASE_ADDR			0x2010
#define FE310_IRQTESTER_0_DSP_2_BASE_ADDR			0x2020
#define FE310_IRQTESTER_0_DSP_3_BASE_ADDR			0x2030
#define FE310_IRQTESTER_0_DSP_0_STATUS_BLOCKED   	2

// below should actually be set by kconfig and end up in auoconf.h
// todo: move to kconfig system
#define CONFIG_IRQTESTER_FE310_0_PRIORITY 	FE310_PLIC_MAX_PRIORITY
#define CONFIG_IRQTESTER_FE310_NAME 		"irqtester0"
#define CONFIG_IRQTESTER_FE310_FAST_IRQ		1
#define CONFIG_IRQTESTER_FE310_FAST_ID2IDX	1	
#if CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
#warning "Fast id2idx functions enabled. No bound checking on indices!"
#endif

/*
 * Enums and macros internal to the driver 
 * ----------------------------------------------------------------------------
 * 
 */

// atomic flags used for sync, reffering to _data.flags
enum flags{
	IRQT_QUEUE_RX_ENABLED,
	IRQT_FIFO_RX_ENABLED,
	IRQT_SEMARR_RX_ENABLED,
	IRQT_VALFLAGS_RX_ENABLED,
	_IRQT_NUM_FLAGS // must be last
};

// Internal helper macros to get handles
#define DEV_CFG(dev)						\
	((const struct irqtester_fe310_config * const)(dev)->config->config_info)
#define DEV_REGS_0(dev)							\
	((volatile struct irqtester_fe310_0_t *)(DEV_CFG(dev))->irqtester_base_addr[0])
#define DEV_REGS_1(dev)							\
	((volatile struct irqtester_fe310_1_t *)(DEV_CFG(dev))->irqtester_base_addr[1])
#define DEV_REGS_2(dev)							\
	((volatile struct irqtester_fe310_2_t *)(DEV_CFG(dev))->irqtester_base_addr[2])
#define DEV_REGS_3(dev)							\
	((volatile struct irqtester_fe310_3_t *)(DEV_CFG(dev))->irqtester_base_addr[3])
#define DEV_DATA(dev)				\
	((struct irqtester_fe310_data *)(dev)->driver_data)
#define DEV()	\
	irqtester_fe310_data0._drv_handle	// call after init only
#define LEN_ARRAY(x) \
	((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
	// call only on arrays defined in scope
	// credits: https://stackoverflow.com/questions/1598773/is-there-a-standard-function-in-c-that-would-return-the-length-of-an-array/1598827#1598827


/*
 * Definition of internal memory pools to store and manage values.
 * Use get_value() to access!
 * - Outside of irqtester_fe310_data. Inside can't be initialized, 
 *   size of array would need to be hard coded.
 * - WARNING: Implementing as static variable means the driver must
 *   be instantiated only once! Move to irqtester_fe310_data if needed.
 * - Auto-generated. Take care when manually adding values to keep
 *   val_name enum from header in sync with _values_<..> arrays.
 * ----------------------------------------------------------------------------
 */
  
// data pool for uint like data
static struct DrvValue_uint _values_uint[] = {
	{._super.id_name = VAL_IRQ_0_PERVAL},
	{._super.id_name = VAL_IRQ_0_VALUE},
	{._super.id_name = VAL_IRQ_0_STATUS},
	{._super.id_name = VAL_IRQ_1_STATUS},
	{._super.id_name = VAL_IRQ_1_PERIOD},
	{._super.id_name = VAL_IRQ_1_NUM_REP},
	{._super.id_name = VAL_IRQ_2_STATUS},
	{._super.id_name = VAL_IRQ_2_PERIOD},
	{._super.id_name = VAL_IRQ_2_NUM_REP},
	{._super.id_name = VAL_DSP_3_STATUS},
	{._super.id_name = VAL_DSP_3_PERVAL},
	{._super.id_name = VAL_DSP_3_ID_COUNTER},
	{._super.id_name = VAL_DSP_3_VALUE},
	{._super.id_name = VAL_DSP_3_PERIOD},
	{._super.id_name = VAL_DSP_3_DUTY},
	{._super.id_name = VAL_DSP_3_NUM_REP},
	{._super.id_name = VAL_DSP_3_DELAY},
	{._super.id_name = VAL_DSP_3_CLEAR_ID},
};

// data pool for int like data
static struct DrvValue_int _values_int[] = {
	
};

// data pool for bool like data
static struct DrvValue_bool _values_bool[] = {
	{._super.id_name = VAL_IRQ_0_ENABLE},
	{._super.id_name = VAL_IRQ_1_CLEAR},
	{._super.id_name = VAL_IRQ_2_CLEAR},
	{._super.id_name = VAL_DSP_3_READY},
	{._super.id_name = VAL_DSP_3_RESET},
};

#define NUM_VALS() \
	(LEN_ARRAY(_values_uint) + LEN_ARRAY(_values_int) +  LEN_ARRAY(_values_bool))


// forward declare static, internal helper functions
static inline irqt_val_type_t id_2_type(irqt_val_id_t id);
static inline irqt_val_type_t id_2_type_fast(irqt_val_id_t id);
static inline int id_2_index(irqt_val_id_t id);
static inline int id_2_index_fast(irqt_val_id_t id, irqt_val_type_t type);

static inline irqt_irq_id_t _get_irq_id();
static inline bool test_flag(struct device * dev, int drv_flag);
static bool test_any_send_flag(struct device * dev);

/*
 * Structs a driver has as expected by the kernel 
 * ----------------------------------------------------------------------------
 * @cond PRIVATE
 */

typedef void (*fe310_cfg_func_t)(void);

/// register-set structure of DSP 0
struct irqtester_fe310_0_t {
	unsigned int value_0;
	unsigned int perval_0;
	unsigned int status_0;
	bool fire_0;	
	bool enable;	// global enable for all DSPs
};
/// register-set structure of DSP 1
struct irqtester_fe310_1_t {
	unsigned int period_1;
	unsigned int num_rep_1;
	unsigned int status_1;
	bool clear_1;
	bool fire_1;
};
/// register-set structure of DSP 2
struct irqtester_fe310_2_t {
	unsigned int period_2;
	unsigned int num_rep_2;
	unsigned int status_2;
	bool clear_2;
	bool fire_2;
};
/// register-set structure of DSP 3
struct irqtester_fe310_3_t {
	unsigned int period_3;
	unsigned int duty_3;
	unsigned int num_rep_3;
	unsigned int delay_3;
	unsigned int value_3;
	unsigned int clear_id_3;
	unsigned int status_3;
	unsigned int perval_3;
	unsigned int id_counter_3;
	bool reset_3;
	bool ready_3;
};

/// config struct for registering with kernel
struct irqtester_fe310_config {
	u32_t               irqtester_base_addr[4];	// base addresses of all DPSs
	fe310_cfg_func_t    irqtester_cfg_func; 	// called at device init, IRQ registering
};


/// data tied to a instance of the driver
struct irqtester_fe310_data {
	/* callback functionss
	 * atm simple void to void function	
	 */
	void (*cb_arr[_NUM_IRQS])(void);
	atomic_t flags; // 32 single bit flags

	// for upward data passing
	// use internally only
	struct device * _drv_handle; 
	struct k_msgq * _queue_rx;
	struct k_fifo * _fifo_rx;
	ATOMIC_DEFINE(_valflags_rx, NUM_VALS());
	// semaphore and shared array for up passing 
	struct k_sem * _sem_rx;
	struct DrvEvent * _evt_arr_rx;
	int _evt_count_rx;
};

/// empty data init
static struct irqtester_fe310_data irqtester_fe310_data0;  

/**@endcond*/



/**
 * IRQ Handler registered to the kernel, just calls a callback.
 * 
 * @param arg: When kernel calls, this is a handle to the device.
 */
static void irqtester_fe310_irq_handler(void *arg){
	//LOG_PERF("[%u] Entering driver handler", get_cycle_32());
	LOG_PERF_INT(2, get_cycle_32());

	// get handles
	struct device *dev = (struct device *)arg;
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	/* Get the irq id generating the interrupt
	   IRQ_0 = 0, IRQ_1 = 1, ... 			*/
	
	irqt_irq_id_t id_irq = _get_irq_id();
	//SYS_LOG_DBG("Irq dispatchter, irq_id: %i \n", id_irq);
	
	if(data->cb_arr[id_irq] != NULL){
		// fire callback
		data->cb_arr[id_irq]();
	}
}


/**
 * ISRs to pass data upwards.
 * 
 * ----------------------------------------------------------------------------
 */

// note: only one kernel object at a time can be enabled
// to send events upwards.
// todo: decide for method and strip away unnecesary parts. Its inside an ISR!
static inline void send_event_rx(struct device * dev, struct DrvEvent * evt){
	#ifndef TEST_MINIMAL

	struct irqtester_fe310_data * data = DEV_DATA(dev);
	bool send_fail = false;

	if(atomic_test_bit( &(data->flags), IRQT_QUEUE_RX_ENABLED )){
		if(0 != k_msgq_put(data->_queue_rx, evt, K_NO_WAIT))
			send_fail = true;
		else{
			//int num_used = k_msgq_num_used_get(data->_queue_rx);
			//int num_free = k_msgq_num_free_get(data->_queue_rx);
			//SYS_LOG_DBG("Sending event %i to queue [%i/%i]", evt->val_id, num_used, num_free + num_used);
			return;
		}
	}

	else if(atomic_test_bit( &(data->flags), IRQT_FIFO_RX_ENABLED )){
		//SYS_LOG_DBG("Sending event %i to fifo", evt->val_id);
		k_fifo_put(data->_fifo_rx, evt);
		return; // success, so get out of here fast
	}
	else if(atomic_test_bit( &(data->flags), IRQT_SEMARR_RX_ENABLED )){
		// sem count indicates array position that are filled with
		// valid DrvEvents
		//SYS_LOG_DBG("Sending semaphore for event %i", evt->val_id);
		int count = k_sem_count_get(data->_sem_rx);
		k_sem_give(data->_sem_rx);			// -> count++
		data->_evt_arr_rx[count] = *evt;	// write event into array
		//SYS_LOG_DBG("Event in array with id %i, type %i", data->_evt_arr_rx[count].val_id, data->_evt_arr_rx[count].val_type);
		//SYS_LOG_DBG("Count %i -> %i", count, k_sem_count_get(data->_sem_rx));
		return; // success, so get out of here fast
	}

	else{
		send_fail = true; // neither queue nor fifo enabled
	}
	
	if(send_fail)
		SYS_LOG_WRN("Couldn't send event with id %i to busy, full or disabled kernel object", evt->val_id);
	
	#endif
}

/**
 *  @brief Sets the (atomic) flag to indicate to higher level applications that
 *  event occured.
 *
 *  Differs from send_event_rx(). No usage of kernel object for synchronization 
 *  (eg. queue, semaphore)
 *  
 */
static inline void flag_event_rx(struct device * dev, struct DrvEvent *evt){
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	if(test_flag(dev, IRQT_VALFLAGS_RX_ENABLED)){
		atomic_set_bit(data->_valflags_rx, evt->val_id);
		return;
	}
	// we don't test for this here, but make sure you're not flagging the 
	// _NIL_VAL 

	else
		SYS_LOG_WRN("Couldn't set flag for event with id %i. Not enabled.", evt->val_id);
}


/**
 * @brief:  Default ISR set up in driver init. Serves all IRQs of irqtester.
 * 			Uses generic getters and conditional logic (potentially slow).
 * 
 * Notes:
 * - Handling all DPSs in one ISR is suboptimal and should be done only
 *   for testing purposes. Better: register seperate handlers for every IRQ.
 * - In zephyr all ISRs have same priority by default.
 * 	 ISR can't be preempted by other ISR.
 * 
 */
// todo: optimize isr parts, avoid non inline function calls
// make getters inline
// check cpu register usage -> should run on register mem only
// make static
void _irq_gen_handler(void){

	#ifndef TEST_MINIMAL

	/* generic part */
	struct device * dev = DEV();
	if(dev == NULL)
		return;	// safety first

	
	u32_t now_cyc = get_cycle_32();

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	struct DrvValue_uint val_uint;
	//struct DrvValue_bool val_bool;

	irqt_irq_id_t irq_id = _get_irq_id();

	// create NIL event structs
	struct DrvEvent evt_val0 = {.val_id=_NIL_VAL, .event_type=EVT_T_VAL_UPDATE, .irq_id=irq_id};
	struct DrvEvent evt_val1 = {.val_id=_NIL_VAL, .event_type=EVT_T_VAL_UPDATE, .irq_id=irq_id};
	// irq events
	struct DrvEvent evt_irq_gen   = {.val_id=_NIL_VAL, .event_type=EVT_T_IRQ, .irq_id=irq_id};

	// note: switch statement bad for branch prediction
	switch(irq_id){
		case IRQ_0:
			// write into internal data pools 
			irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &val_uint);
			_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = val_uint.payload;
			_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_cyc = now_cyc; 

			irqtester_fe310_get_reg(dev, VAL_IRQ_0_STATUS, &val_uint);
			_values_uint[id_2_index(VAL_IRQ_0_STATUS)].payload = val_uint.payload;
			_values_uint[id_2_index(VAL_IRQ_0_STATUS)]._super.time_cyc = now_cyc;

			// fill event
			evt_val0.val_id 	= VAL_IRQ_0_PERVAL;
			evt_val0.val_type 	= VAL_T_UINT;
			evt_val1.val_id 	= VAL_IRQ_0_STATUS;
			evt_val1.val_type 	= VAL_T_UINT;
			break;

		case IRQ_1:
			irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_uint);
			_values_uint[id_2_index(VAL_IRQ_1_STATUS)].payload = val_uint.payload;
			_values_uint[id_2_index(VAL_IRQ_1_STATUS)]._super.time_cyc = now_cyc;

			evt_val0.val_id 	= VAL_IRQ_1_STATUS;
			evt_val0.val_type	= VAL_T_UINT;
			// clear irq hardware on hw rev 2
			// ISRs can't be preempted, so don't have to do at end 
			irqtester_fe310_clear_1(dev);
			break;
		case IRQ_2:	
			irqtester_fe310_get_reg(dev, VAL_IRQ_2_STATUS, &val_uint);
			_values_uint[id_2_index(VAL_IRQ_2_STATUS)].payload = val_uint.payload;
			_values_uint[id_2_index(VAL_IRQ_2_STATUS)]._super.time_cyc = now_cyc;

			evt_val0.val_id 	= VAL_IRQ_2_STATUS;
			evt_val0.val_type	= VAL_T_UINT;
			irqtester_fe310_clear_2(dev);
			break;
		default:
			SYS_LOG_ERR("Unkown irq id %i. Check driver init.", irq_id);
			return;

	}

	// inefficient to test bit here, but this is a default ISR for testing only
	if(test_any_send_flag(dev)){
		send_event_rx(dev, &evt_irq_gen);
		if(evt_val0.val_id != _NIL_VAL)
			send_event_rx(dev, &evt_val0);
		if(evt_val1.val_id != _NIL_VAL)
			send_event_rx(dev, &evt_val1);
	}
	if(test_flag(dev, IRQT_VALFLAGS_RX_ENABLED)){
		if(evt_val0.val_id != _NIL_VAL)
			flag_event_rx(dev, &evt_val0);
		if(evt_val1.val_id != _NIL_VAL)
			flag_event_rx(dev, &evt_val1);	
	}
	
	#endif // TEST_MINIMAL
	
}

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
	u32_t now_cyc = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
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
	struct DrvValue_uint perval_0;
	struct DrvValue_bool enable;
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

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	
	struct DrvEvent evt_irq1   = {.val_id=_NIL_VAL, .event_type=EVT_T_IRQ, .irq_id=IRQ_1};

	// manually inlining send, flag functions
	//if(0 != k_msgq_put(data->_queue_rx, &evt_irq1, K_NO_WAIT))
	//	SYS_LOG_WRN("Couln't send event irq1 to driver queue");
	k_msgq_put(data->_queue_rx, &evt_irq1, K_NO_WAIT);
	//SYS_LOG_WRN("IRQ1");

	// clear irq hardware on hw rev 2
	irqtester_fe310_clear_1(dev);
}

// hand optimized handler irq1, queue, no clear
void _irq_1_handler_1(void){

	struct device * dev = DEV();
	struct irqtester_fe310_data * data = DEV_DATA(dev);

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	
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





/*
 * Public function available to applications. 
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Thread safe generic (by id) getter for values from driver memory pools.
 * 
 * Doesn't read from registers; an irq is needed to load regs into driver mem.
 * @param id: id of value to get
 * @param res_value: pointer to a DrvValue_X struct of correct type. 
 * @return always 0
 */ 
int irqtester_fe310_get_val(irqt_val_id_t id, void * res_value){
	
	// we only read data -> thread safe without synchronization

	#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
	irqt_val_type_t type = id_2_type_fast(id);
	#else
	irqt_val_type_t type = id_2_type(id);
	#endif

	int res = 0;
	//SYS_LOG_DBG("Getting value for id %i, type %i", id, type);

	/* enter CRITICAL SECTION
	 * ISRs write to internal value pools
	 * we must not be interrupted while reading these 
	 */ 
	unsigned int lock_key = irq_lock();

	switch(type){
		case VAL_T_UINT:;
			//struct DrvValue_uint * dbg = ((struct DrvValue_uint *) res_value);
			((struct DrvValue_uint *) res_value)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			((struct DrvValue_uint *) res_value)->payload = _values_uint[id_2_index_fast(id, VAL_T_UINT)].payload;
			((struct DrvValue_uint *) res_value)->_super.time_cyc = _values_uint[id_2_index_fast(id, VAL_T_UINT)]._super.time_cyc;
		#else
			((struct DrvValue_uint *) res_value)->payload = _values_uint[id_2_index(id)].payload;
			((struct DrvValue_uint *) res_value)->_super.time_cyc = _values_uint[id_2_index(id)]._super.time_cyc;
		#endif
			//SYS_LOG_DBG("at %p payload %i, time %i", dbg, dbg->payload, dbg->_super.time_cyc);
			break;
		case VAL_T_INT:
			((struct DrvValue_int *) res_value)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			((struct DrvValue_int *) res_value)->payload = _values_int[id_2_index_fast(id, VAL_T_INT)].payload;
			((struct DrvValue_int *) res_value)->_super.time_cyc = _values_int[id_2_index_fast(id, VAL_T_INT)]._super.time_cyc;
		#else
			((struct DrvValue_int *) res_value)->payload = _values_int[id_2_index(id)].payload;
			((struct DrvValue_int *) res_value)->_super.time_cyc = _values_int[id_2_index(id)]._super.time_cyc;
		#endif
			break;
		case VAL_T_BOOL:
			((struct DrvValue_bool *) res_value)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			((struct DrvValue_bool *) res_value)->payload = _values_bool[id_2_index_fast(id, VAL_T_BOOL)].payload;
			((struct DrvValue_bool *) res_value)->_super.time_cyc = _values_bool[id_2_index_fast(id, VAL_T_BOOL)]._super.time_cyc;
		#else
			((struct DrvValue_bool *) res_value)->payload = _values_bool[id_2_index(id)].payload;
			((struct DrvValue_bool *) res_value)->_super.time_cyc = _values_bool[id_2_index(id)]._super.time_cyc;
		#endif
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
			res = 1;
	}	

	irq_unlock(lock_key);

	return res;
		 
}

/** 
 * @brief 	Non-Thread safe, generic setter for device registers.
 * 			Only works for regs which have a driver mem pool entry.	
 * 
 * @param id: id of value to set
 * @param set_value: pointer to a DrvValue_X struct of correct type. 
 */ 
int irqtester_fe310_set_reg_fast(struct device * dev, irqt_val_id_t id, void * set_val){
	int retval = 0;
	
	#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
	irqt_val_type_t type = id_2_type_fast(id);
	#else
	irqt_val_type_t type = id_2_type(id);
	#endif

	// catch zero dereferencing if base_addr wasn't stored
	volatile void * addr;

	/* enter CRITICAL SECTION
	 * we must not be interrupted while setting these 
	 */ 

	unsigned int lock_key = irq_lock();

	// cast the given value container depending on 
	// its type to set the value in corresponding memory pool
	switch(type){
		case VAL_T_UINT: 
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = _values_uint[id_2_index_fast(id, VAL_T_UINT)].base_addr;
		#else
			addr = _values_uint[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 2;
			else
				*((u32_t *)addr) = ((struct DrvValue_uint *)set_val)->payload;
			break;
		case VAL_T_INT: 
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = _values_int[id_2_index_fast(id, VAL_T_INT)].base_addr;
		#else
			addr = _values_int[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 2;
			else
				*((int *)addr) = ((struct DrvValue_int *)set_val)->payload;
			break;
		case VAL_T_BOOL: 
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = _values_bool[id_2_index_fast(id, VAL_T_BOOL)].base_addr;
		#else
			addr = _values_bool[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 2;
			else
				*((bool *)addr) = ((struct DrvValue_bool *)set_val)->payload;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
	}
	
	irq_unlock(lock_key);
	
	if(retval == 2)
		SYS_LOG_WRN("NULL base_addr for val id %i. Check _store_reg_adr() in driver init.", id);


	return retval;
}

int irqtester_fe310_set_reg_uint_fast(struct device * dev, irqt_val_id_t id, void * set_val){
	int retval = 0;
	
	// catch zero dereferencing if base_addr wasn't stored
	volatile void * addr;

	/* enter CRITICAL SECTION
	 * we must not be interrupted while setting these 
	 */ 

	//printk("Debug: set_reg to _values_uint[%i] @ %p\n", id_2_index_fast(id, VAL_T_UINT), _values_uint);
	unsigned int lock_key = irq_lock();

#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
	addr = _values_uint[id_2_index_fast(id, VAL_T_UINT)].base_addr;
#else
	addr = _values_uint[id_2_index(id)].base_addr;
#endif
	if(addr == NULL){
		SYS_LOG_WRN("NULL base_addr for val id %i. Check _store_reg_adr() in driver init.", id);
		retval = 2;
	}
	else
		*((u32_t *)addr) = ((struct DrvValue_uint *)set_val)->payload;
		
	
	irq_unlock(lock_key);
			

	return retval;
}

/** 
 * @brief 	Thread safe, generic setter for device registers.
 * 			Only works for regs which have a driver mem pool entry.	
 * 
 * Must not be called by an ISR (mutex).
 * @param id: id of value to set
 * @param set_value: pointer to a DrvValue_X struct of correct type. 
 */ 
int irqtester_fe310_set_reg(struct device * dev, irqt_val_id_t id, void * set_val){
	
	// only one thread a time can write to regs
	static struct k_mutex lock;
	k_mutex_init(&lock);

	if (k_mutex_lock(&lock, K_MSEC(1)) != 0) {
		SYS_LOG_WRN("Couldn't obtain lock to set registers with id %i", id);
		return 1;
	}

	int res = irqtester_fe310_set_reg_fast(dev, id, set_val);

	k_mutex_unlock(&lock);

	return res;

}

/**
 * @brief 	Non-thread safe, generic getter for device (output) registers.
 * 			Only works for regs which are loaded to driver mem pools.			
 * 
 * Might be called from an ISR, but potentially slower than direct getters.
 * For an reg to be available, _store_reg_addr() has to be called in driver init.
 * @param id: id of value to set
 * @param set_value: pointer to a DrvValue_X struct of correct type. 
 */ 
int irqtester_fe310_get_reg(struct device * dev, irqt_val_id_t id, void * res_val){
	
	#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
	irqt_val_type_t type = id_2_type_fast(id);
	#else
	irqt_val_type_t type = id_2_type(id);
	#endif

	u32_t now_cyc = get_cycle_32();
	
	int retval = 0;
	void * addr;

	/* enter CRITICAL SECTION
	 * we must not be interrupted while getting these 
	 */ 
	
	unsigned int lock_key = irq_lock();
	
	// cast the given value container depending on its type 
	switch(type){
		case VAL_T_UINT:
		((struct DrvValue_uint *) res_val)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = (void *)_values_uint[id_2_index_fast(id, VAL_T_UINT)].base_addr;
		#else
			addr = (void *)_values_uint[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 1;
			else
				((struct DrvValue_uint *) res_val)->payload = *((u32_t *)addr);
			((struct DrvValue_uint *) res_val)->_super.time_cyc = now_cyc;
			break;
		case VAL_T_INT:
			((struct DrvValue_int *) res_val)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = (void *)_values_uint[id_2_index_fast(id, VAL_T_INT)].base_addr;
		#else
			addr = (void *)_values_int[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 1;
			else
				((struct DrvValue_int *) res_val)->payload = *((int *)addr);
			((struct DrvValue_int *) res_val)->_super.time_cyc = now_cyc;
			break;
		case VAL_T_BOOL:
			((struct DrvValue_bool *) res_val)->_super.id_name = id;
		#if	CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 0
			addr = (void *)_values_uint[id_2_index_fast(id, VAL_T_BOOL)].base_addr;
		#else
			addr = (void *)_values_bool[id_2_index(id)].base_addr;
		#endif
			if(addr == NULL)
				retval = 1;
			else
				((struct DrvValue_bool *) res_val)->payload = *((bool *)addr);
			((struct DrvValue_bool *) res_val)->_super.time_cyc = now_cyc;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
	}
	
	irq_unlock(lock_key);

	if(retval == 1)
		SYS_LOG_WRN("NULL base_addr for val id %i. Check _store_reg_adr() in driver init.", id);

	return retval;

}



/**
 * @brief Test whether a value was flagged ready to get_val() from driver memory. 
 * @param id: id_name of value
 */
bool irqtester_fe310_test_valflag(struct device *dev, irqt_val_id_t id){
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	if(id > _NUM_VALS){
		SYS_LOG_WRN("Requesting to test flag for invalid value id %i", id);
		return false;
	}
	return atomic_test_bit(data->_valflags_rx, id);
}


/**
 * @brief Clear all flags.
 */
void irqtester_fe310_clear_all_valflags(struct device *dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	atomic_clear(data->_valflags_rx);
}

/**
 * @brief Provide a handle to a queue to receive msgs from the driver there.
 * 
 * Remember to enable_queue_rx() and to empty it in the consuming thread. 
 * @param k_msgsq: pointer to queue
 */
int irqtester_fe310_register_queue_rx(struct device * dev, struct k_msgq * queue){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_queue_rx == NULL) 
		data->_queue_rx = queue;
	else{
		SYS_LOG_WRN("Can't register more than one rx queue");
		return 1;
	}
	return 0;
}

/**
 * @brief Provide a handle to a fifo to receive msgs from the driver there.
 * 
 * Remember to enable_fifo_rx() and to empty it in the consuming thread. 
 * Attention: Using a fifo is not recommended, as it is not necessarily
 * faster and prone to causing overflows (unlimited size!).
 */
int irqtester_fe310_register_fifo_rx(struct device * dev, struct k_fifo * queue){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_fifo_rx == NULL) 
		data->_fifo_rx = queue;
	else{
		SYS_LOG_WRN("Can't register more than one rx fifo");
		return 1;
	}
	return 0;
}

/**
 * @brief Provide a handle to a semaphore and an even array to receive msgs from the driver there.
 * 
 * Remember to enable_fifo_rx() and to empty it in the consuming thread. 
 */
int irqtester_fe310_register_sem_arr_rx(struct device * dev, 
						struct k_sem * sem, struct DrvEvent evt_arr[], int len){
	
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_sem_rx == NULL){
		data->_sem_rx = sem;
		data->_evt_arr_rx = evt_arr;
		data->_evt_count_rx = len;
	}
	else{
		SYS_LOG_WRN("Can't register more than one rx semaphore plus array combination");
		return 1;
	}
	return 0;
}

/**
 * @brief 	Test whether any flags for event sending are set.
 * 			This excludes valflags, as they don't issue events.
 */
static bool test_any_send_flag(struct device * dev){
	
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	bool any = false;

	for(int i=0; i < _IRQT_NUM_FLAGS; i++){
		if(i != IRQT_VALFLAGS_RX_ENABLED)
			any = atomic_test_bit(&(data->flags), i);
		if(any)
			return true;
	}
	return any;
}

/**
 * @brief Enable previously registered queue_rx for sending driver events to.
 */
int irqtester_fe310_enable_queue_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_queue_rx == NULL){
		SYS_LOG_WRN("No queue to enable");
		return 1;
	}


	if(test_any_send_flag(dev)){
		SYS_LOG_WRN("Purging rx to enable queue exclusively.");
		irqtester_fe310_purge_rx(dev);
	}

	atomic_set_bit(&(data->flags), IRQT_QUEUE_RX_ENABLED);

	SYS_LOG_DBG("Enabled driver queue rx @%p", data->_queue_rx);

	return 0;
}

/**
 * @brief Enable previously registered fifo_rx for sending driver events to.
 */
int irqtester_fe310_enable_fifo_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_fifo_rx == NULL){
		SYS_LOG_WRN("No fifo to enable");
		return 1;
	}


	if(test_any_send_flag(dev)){
		SYS_LOG_WRN("Purging rx to enable fifo exclusively.");
		irqtester_fe310_purge_rx(dev);
	}

	atomic_set_bit(&(data->flags), IRQT_FIFO_RX_ENABLED);

	return 0;
}

/**
 * @brief Enable previously registered semaphore plus event array for sending driver events to.
 */
int irqtester_fe310_enable_sem_arr_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_sem_rx == NULL){
		SYS_LOG_WRN("No semaphore plus array to enable");
		return 1;
	}

	if(test_any_send_flag(dev)){
		SYS_LOG_WRN("Purging rx to enable semaphore plus array exclusively.");
		irqtester_fe310_purge_rx(dev);
	}

	atomic_set_bit(&(data->flags), IRQT_SEMARR_RX_ENABLED);

	return 0;
}

/**
 * @brief Enables flags for indicating load of harware values to driver memory.
 * 
 * Since they don't issue events, valflags can be enabled in parallel to other sending flags
 * which do issue events.
 */

int irqtester_fe310_enable_valflags_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	atomic_set_bit(&(data->flags), IRQT_VALFLAGS_RX_ENABLED);

	return 0;
}



/**
 * @brief If using semaphore plus event array, call this to wait and receive an event.
 * 
 * Should be called from a single thread only.
 * Todo: add example for usage
 */
int irqtester_fe310_receive_evt_from_arr(struct device * dev, struct DrvEvent * res, s32_t timeout){
	
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if (data->_sem_rx == NULL || !atomic_test_bit(&(data->flags), IRQT_SEMARR_RX_ENABLED)){
		SYS_LOG_ERR("Semaphore not ready. Correctly set up?");
		return 2;
	}

	if(k_sem_take(data->_sem_rx, timeout) != 0){
		//SYS_LOG_WRN("Semaphore busy or timed out.");
		return 1;	
	}
	int count = k_sem_count_get(data->_sem_rx);

	data = DEV_DATA(dev);
	/* Enter CRITICAL 
	 * ISRs write to this shared memory array, we must not be interrupted reading.
	 */
	unsigned int lockkey = irq_lock();
	*res = data->_evt_arr_rx[count];
	irq_unlock(lockkey);
	
	if(res->cleared != 0){
		SYS_LOG_WRN("Event with id %i at count %i shouln't be cleared yet!", res->val_id, count);
	}
	else
		data->_evt_arr_rx[count].cleared++;

	return 0;
}

/**
 * @brief Reset all objects related to receiving data from this driver.
 */
int irqtester_fe310_purge_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	// clear first to not access NULL pointers in ISRs
	atomic_clear(data->_valflags_rx);

	atomic_clear_bit(&(data->flags), IRQT_QUEUE_RX_ENABLED);
	atomic_clear_bit(&(data->flags), IRQT_FIFO_RX_ENABLED);
	atomic_clear_bit(&(data->flags), IRQT_SEMARR_RX_ENABLED);
	atomic_clear_bit(&(data->flags), IRQT_VALFLAGS_RX_ENABLED);

	data->_queue_rx = NULL;
	data->_fifo_rx = NULL;
	data->_sem_rx = NULL;
	data->_evt_arr_rx = NULL;
	data->_evt_count_rx = 0;


	return 0;
}

/**
 * @brief Print information about an event as a debug log message.
 */
void irqtester_fe310_dbgprint_event(struct device * dev, struct DrvEvent * evt){

	irqt_val_id_t id = evt->val_id;

	SYS_LOG_DBG("Driver event [type / value type]: %i, %i", evt->event_type, evt->val_type);
	
	if(evt->event_type == EVT_T_VAL_UPDATE){
		if(evt->val_type == VAL_T_INT){
			struct DrvValue_int val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val._super.id_name, val.payload, val._super.time_cyc);
		} 
		else if(evt->val_type == VAL_T_UINT){
			struct DrvValue_uint val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %u updated at %u ns", val._super.id_name, val.payload, val._super.time_cyc);
		} 
		else if(evt->val_type == VAL_T_BOOL){
			struct DrvValue_bool val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val._super.id_name, val.payload, val._super.time_cyc);
		}
		else
			SYS_LOG_WRN("Can't print event of unknown value type %i", evt->val_type);
	}
}

/*
 * Internal driver functions. 
 * First argument must always be handle to driver.
 * @cond PRIVATE
 * ----------------------------------------------------------------------------
 */

// TODO: encapsulate hardware reads, make functions static

/**
 * @brief: Fast id 2 index functions. 
 * 
 * Used if CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 1 set.
 * No bound checking! Make sure there is NEVER an id < 0 or id > _NUM_VALS 
 */
#define IS_GREATER(x, y) \
	((int)(x) > (int)(y) ? 1 : 0)
#define ID_2_INDEX(id, type) \
	(id - (IS_GREATER(type, VAL_T_UINT))*(LEN_ARRAY(_values_uint)) \
		- (IS_GREATER(type, VAL_T_INT ))*(LEN_ARRAY(_values_int)) - 1)
#define ID_2_TYPE(id) \
	(_NUM_VAL_TYPES - (!(IS_GREATER(id, LEN_ARRAY(_values_uint))) * 1) \
					- (!(IS_GREATER(id, LEN_ARRAY(_values_uint) + LEN_ARRAY(_values_int))) * 1) - 1)
static int id_2_index_fast(irqt_val_id_t id, irqt_val_type_t type){
	return ID_2_INDEX(id, type);
}
static irqt_val_type_t id_2_type_fast(irqt_val_id_t id){
	return ID_2_TYPE(id);
}


/**
 * @brief: Slow but safe id 2 index functions. 
 * 
 * Used if CONFIG_IRQTESTER_FE310_FAST_ID2IDX > 1 not set.
 * Bound cheking performed, but many branch instructions.
 * Calc index to access the _values_ arrays from value id
 * Order of _values_id must match .h::irqt_val_id_t
 */
static inline irqt_val_type_t id_2_type(irqt_val_id_t id){

	int n_uint = LEN_ARRAY(_values_uint);
	int m_int = LEN_ARRAY(_values_int);
	int k_bool = LEN_ARRAY(_values_bool);

	if(id > 0 && (id < n_uint + 1)){	// +1 -> id 0=_NIL_VAL
		//SYS_LOG_DBG("Returning type %i", VAL_T_UINT);
		return VAL_T_UINT;
	} 
	else if(id >= n_uint + 1 && id < (n_uint + m_int + 1)){
		//SYS_LOG_DBG("Returning type %i", VAL_T_INT);
		return VAL_T_INT;
	}
	else if(id >= (n_uint + m_int + 1)  && id < (n_uint + m_int + k_bool + 1)){
		//SYS_LOG_DBG("Returning type %i", VAL_T_BOOL);
		return VAL_T_BOOL;
	}
	else{
		SYS_LOG_ERR("Tried to get type for unknown id %i", id);
		return -1;
	}
	
}

static inline int id_2_index(irqt_val_id_t id){
	int n_uint = LEN_ARRAY(_values_uint);
	int m_int = LEN_ARRAY(_values_int);
	//int k_bool = LEN_ARRAY(_values_bool);

	irqt_val_type_t type = id_2_type(id);

	switch(type){
		// +1 -> id 0=_NIL_VAL
		case VAL_T_UINT:	
			//SYS_LOG_DBG("Returning idx %i", id);
			return id - 1; // uint
		case VAL_T_INT:
			//SYS_LOG_DBG("Returning idx %i", id - n_uint);
			return id - n_uint - 1; // int
		case VAL_T_BOOL:
			//SYS_LOG_DBG("Returning idx %i", id - n_uint - m_int);
			return id - n_uint - m_int - 1; // bool
		default:
			SYS_LOG_ERR("Tried to get index for unknown id %i", id);
			return -1;
	}
}

/**
 * @brief: Call ONLY from ISR!
 */
static inline irqt_irq_id_t _get_irq_id(){
	//return 0; // debug only
	return riscv_plic_get_irq() - FE310_IRQTESTER_0_IRQ_0_PIN;
}
	


// helper
static inline bool test_flag(struct device * dev, int drv_flag){
	
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	return atomic_test_bit( &(data->flags), drv_flag);
}

static int _store_reg_addr(irqt_val_id_t id, volatile void * addr){
	
	irqt_val_type_t type = id_2_type(id);
	//SYS_LOG_DBG("Saving addr for value id %i: %p", id, addr);

	switch(type){
		case VAL_T_UINT:
			_values_uint[id_2_index(id)].base_addr = (u32_t *)addr;
			break;
		case VAL_T_INT:
			_values_int[id_2_index(id)].base_addr = (int *)addr;
			break;
		case VAL_T_BOOL:
			_values_bool[id_2_index(id)].base_addr = (bool *)addr;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
			return 1;
	}
	
	return 0;
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

/**
 * @brief Initialize driver
 * @param dev IRQTester device struct
 *
 * @return 0
 */
static int irqtester_fe310_init(struct device *dev)
{
	// export global driver pointer
	g_dev_cp = dev;

	// get handles to registers of all DPSs
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);
	volatile struct irqtester_fe310_1_t *irqt_1 = DEV_REGS_1(dev);
	volatile struct irqtester_fe310_2_t *irqt_2 = DEV_REGS_2(dev);
	volatile struct irqtester_fe310_3_t *irqt_3 = DEV_REGS_3(dev);
	const struct irqtester_fe310_config *cfg = DEV_CFG(dev);
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	SYS_LOG_DBG("Init iqrtester driver with hw @ %p, %p, %p, %p \n" \
					"\t mem pools (uint, int, bool) @ %p, %p, %p", 
				irqt_0, irqt_1, irqt_2, irqt_3, _values_uint, _values_int, _values_bool);

	/* Ensure that all registers are reset to 0 initially */
	irqt_0->enable		= 0;
	irqt_0->value_0		= 0;
	irqt_0->perval_0	= 0;
	irqt_0->status_0	= 0;
	irqt_0->fire_0   	= 0;

	irqt_1->period_1	= 0;
	irqt_1->num_rep_1	= 0;
	irqt_1->status_1	= 0;
	irqt_1->fire_1   	= 0;
	irqt_1->clear_1		= 0;

	irqt_2->period_2	= 0;
	irqt_2->num_rep_2	= 0;
	irqt_2->status_2	= 0;
	irqt_2->fire_2   	= 0;
	irqt_2->clear_2		= 0;

	irqt_3->period_3	= 0;
	irqt_3->duty_3		= 0;
	irqt_3->num_rep_3	= 0;
	irqt_3->delay_3		= 0;
	irqt_3->value_3		= 0;
	irqt_3->clear_id_3	= 0;
	irqt_3->status_3	= 0;
	irqt_3->perval_3	= 0;
	irqt_3->id_counter_3= 0;
	irqt_3->reset_3		= 0;
	irqt_3->ready_3		= 0;

	// store addr of registers to allow set/get by id_name
	_store_reg_addr(VAL_IRQ_0_ENABLE, 	&(irqt_0->enable)	);
	_store_reg_addr(VAL_IRQ_0_VALUE, 	&(irqt_0->value_0)	);
	_store_reg_addr(VAL_IRQ_0_PERVAL, 	&(irqt_0->perval_0)	);
	_store_reg_addr(VAL_IRQ_0_STATUS, 	&(irqt_0->status_0)	);

	_store_reg_addr(VAL_IRQ_1_STATUS, 	&(irqt_1->status_1)	);
	_store_reg_addr(VAL_IRQ_1_PERIOD, 	&(irqt_1->period_1)	);
	_store_reg_addr(VAL_IRQ_1_NUM_REP, 	&(irqt_1->num_rep_1));
	_store_reg_addr(VAL_IRQ_1_CLEAR, 	&(irqt_1->clear_1)	);
	
	_store_reg_addr(VAL_IRQ_2_STATUS, 	&(irqt_2->status_2)	);
	_store_reg_addr(VAL_IRQ_2_PERIOD, 	&(irqt_2->period_2)	);
	_store_reg_addr(VAL_IRQ_2_NUM_REP, 	&(irqt_2->num_rep_2));
	_store_reg_addr(VAL_IRQ_2_CLEAR, 	&(irqt_2->clear_2)	);

	_store_reg_addr(VAL_DSP_3_VALUE, 	&(irqt_3->value_3)	);
	_store_reg_addr(VAL_DSP_3_PERVAL, 	&(irqt_3->perval_3)	);
	_store_reg_addr(VAL_DSP_3_STATUS, 	&(irqt_3->status_3)	);
	_store_reg_addr(VAL_DSP_3_ID_COUNTER, &(irqt_3->id_counter_3));
	_store_reg_addr(VAL_DSP_3_PERIOD, 	&(irqt_3->period_3)	);
	_store_reg_addr(VAL_DSP_3_DUTY, 	&(irqt_3->duty_3)	);
	_store_reg_addr(VAL_DSP_3_NUM_REP, 	&(irqt_3->num_rep_3));
	_store_reg_addr(VAL_DSP_3_DELAY, 	&(irqt_3->delay_3)	);
	_store_reg_addr(VAL_DSP_3_CLEAR_ID, &(irqt_3->clear_id_3));
	_store_reg_addr(VAL_DSP_3_READY, 	&(irqt_3->ready_3)	);
	_store_reg_addr(VAL_DSP_3_RESET, 	&(irqt_3->reset_3)	);

	/* Setup IRQ handler  */
	// Strange that not done by kernel...
	cfg->irqtester_cfg_func();

	// default handler 
	#ifndef TEST_MINIMAL
	// 
	irqtester_fe310_register_callback(dev, 0, _irq_gen_handler);
	irqtester_fe310_register_callback(dev, 1, _irq_gen_handler);
	irqtester_fe310_register_callback(dev, 2, _irq_gen_handler);
	#else
	irqtester_fe310_register_callback(dev, 0, _irq_0_handler_5);
	#endif
	// store handle to driver in data for internal calls
	data->_drv_handle = dev;

	return 0;
}

// note: it must be guaranteed that handler is NEVER 
// called when corresping cb_arr == NULL 
// note: to be fast, data must be placed in DTIM region!
static void plic_fast_handler(int irq_num){
	struct device * dev = DEV(); 
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	//SYS_LOG_DBG("ISR called with irq num %i", irq_num);

	// do pointer arithmetic to be fast
	int * cbs_p =(int *) &(data->cb_arr);
	// cast to function pointer and call with correct offset
	// irq_num is guaranteed to be within array index, if 
	// driver setup was correct
	(( void(*)(void) )cbs_p[irq_num - FE310_IRQTESTER_0_IRQ_0_PIN])();


	/*
	switch(irq_num){
		case FE310_IRQTESTER_0_IRQ_0 - RISCV_MAX_GENERIC_IRQ:	
			cb_irq0();
			//data->cb_arr[0]();
			return;
		case FE310_IRQTESTER_0_IRQ_1 - RISCV_MAX_GENERIC_IRQ:
			cb_irq1();
			//data->cb_arr[1]();
			return;
		case FE310_IRQTESTER_0_IRQ_2 - RISCV_MAX_GENERIC_IRQ:
			cb_irq2();
			//data->cb_arr[2]();
			return;
		default:
			SYS_LOG_ERR("ISR called with invalid irq num %i", irq_num);
			//_irq_spurious(NULL);
	}
	*/
}

int irqtester_fe310_register_callback(struct device *dev, irqt_irq_id_t irq_id, void (*cb)(void)){	

	// get handle to data and write cb into
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	
	data->cb_arr[irq_id] = cb;


	/* Enable interrupt for the pin at PLIC level */
	switch(irq_id){
		case IRQ_0:
			irq_enable(FE310_IRQTESTER_0_IRQ_0);
			break;
		case IRQ_1:
			irq_enable(FE310_IRQTESTER_0_IRQ_1);
			break;
		case IRQ_2:
			irq_enable(FE310_IRQTESTER_0_IRQ_2);
			break;
		default:
			SYS_LOG_WRN("Unknown IRQ id: %i", irq_id);
			return 1;
	}
	
	return 0;
}


int irqtester_fe310_unregister_callback(struct device *dev, irqt_irq_id_t irq_id){	
	// get handle to data and write cb into
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	
	/* Disable interrupt at PLIC level */
	switch(irq_id){
		case IRQ_0:
			irq_disable(FE310_IRQTESTER_0_IRQ_0);
			break;
		case IRQ_1:
			irq_disable(FE310_IRQTESTER_0_IRQ_1);
			break;
		case IRQ_2:
			irq_disable(FE310_IRQTESTER_0_IRQ_2);
			break;
		default:
			SYS_LOG_WRN("Unknown IRQ id: %i", irq_id);
			return 1;
	}
	
	data->cb_arr[irq_id] = NULL;

	return 0;
}

/*
 * Non generic, non thread-safe direct setters for registers.
 * Avoid to call from application code.
 * ----------------------------------------------------------------------------
 */

/* DEPRECATED
int irqtester_fe310_set_value(struct device *dev, unsigned int val)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	irqt_0->value_0 = val;
	//printk("Setting value_0 %i to %p \n", val, &(irqt_0->value_0));
	//printk("value_0: %i \n", irqt_0->value_0);

	return 0;
}

int irqtester_fe310_get_value(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	*res = irqt_0->value_0;
	//printk("Getting value_0 %i from %p \n", *res, &(irqt_0->value_0));

	return 0;
}
*/
int irqtester_fe310_get_enable(struct device *dev, bool * res)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	*res = irqt_0->enable;

	return 0;
}

inline int irqtester_fe310_get_perval(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	*res = irqt_0->perval_0;

	return 0;
}
/*
int irqtester_fe310_get_status(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	*res = irqt_0->status_0;

	return 0;
}

int irqtester_fe310_set_enable(struct device *dev, bool val)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	irqt_0->enable = val;

	return 0;
}
*/

int irqtester_fe310_enable(struct device *dev)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	irqt_0->enable = 1;

	return 0;
}

int irqtester_fe310_disable(struct device *dev)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	irqt_0->enable = 0;

	return 0;
}

/**@endcond*/ 

int irqtester_fe310_fire(struct device *dev)
{	
	volatile struct irqtester_fe310_0_t *irqt_0 = DEV_REGS_0(dev);

	// device goes to status blocked after fire_0
	// to indicate that fired once and blocked after
	irqt_0->fire_0 = 1;
	int timeout = 100; //cpu cycles
	while(irqt_0->status_0 == FE310_IRQTESTER_0_DSP_0_STATUS_BLOCKED && timeout >= 0){
		// we (probably) can't k_wait() here, because may be called outside a thread
		timeout -= 1;
	}

	// rearm blocked (status 1) hardware
	irqt_0->fire_0 = 0;

	if(timeout < 0){
		return 1;
		// hardware did not respond to fire command
	}

	timeout = 100;
	while(irqt_0->status_0 != 0 && timeout >= 0){
		timeout -= 1;
	}

	if(timeout < 0){
		return 2;
		// wan't able to unblock device
	}

	return 0;
}

/**
 * @brief:
 * 
 * Need to set regs num_rep_1 and period_1 first.
 */
void irqtester_fe310_fire_1(struct device *dev){	
	/*
	volatile struct irqtester_fe310_1_t *irqt_1 = DEV_REGS_1(dev);

	// is saved to internal register after 1 clock cycle
	irqt_1->fire_1 = 1;
	// reset
	irqt_1->fire_1 = 0;

	return 0;
	*/
	#define IRQ1_FIRE  0x201D
	reg_write_short(IRQ1_FIRE, 1);
    reg_write_short(IRQ1_FIRE, 0);
}

/**
 * @brief:
 * 
 * Need to set regs num_rep_2 and period_2 first.
 */
int irqtester_fe310_fire_2(struct device *dev){	
	
	volatile struct irqtester_fe310_2_t *irqt_2 = DEV_REGS_2(dev);

	// is saved to internal register after 1 clock cycle
	irqt_2->fire_2 = 1;
	// reset
	irqt_2->fire_2 = 0;

	return 0;
}

/**
 * @brief:
 * 
 */
void irqtester_fe310_clear_1(struct device *dev){	
	// SLOW OLD CODE
	/*
	struct device * dev = DEV();
	volatile struct irqtester_fe310_1_t *irqt_1 = DEV_REGS_1(dev);

	// is saved to internal register after 1 clock cycle
	irqt_1->clear_1 = 1;
	// reset
	irqt_1->clear_1 = 0;
	*/
	// FAST BUT UGLY, todo: make nice
	#define IRQ1_CLEAR  0x201C
	reg_write_short(IRQ1_CLEAR, 1);
    reg_write_short(IRQ1_CLEAR, 0);
}

/**
 * @brief:
 * 
 */
void irqtester_fe310_clear_2(struct device *dev){
	// SLOW OLD CODE
	/*	
	volatile struct irqtester_fe310_2_t *irqt_2 = DEV_REGS_2(dev);

	// is saved to internal register after 1 clock cycle
	irqt_2->clear_2 = 1;
	// reset
	irqt_2->clear_2 = 0;

	return 0;
	*/
	#define IRQ2_CLEAR  0x202C
	reg_write_short(IRQ2_CLEAR, 1);
    reg_write_short(IRQ2_CLEAR, 0);
}




/* 
 * Code below sets up and registers driver with kernel, no driver API used
 * @cond PRIVATE
 * ----------------------------------------------------------------------------
 */

static void irqtester_fe310_cfg_0(void); // forward declared

static const struct irqtester_fe310_config irqtester_fe310_config0 = {
	.irqtester_base_addr    = {FE310_IRQTESTER_0_DSP_0_BASE_ADDR,
							   FE310_IRQTESTER_0_DSP_1_BASE_ADDR,
							   FE310_IRQTESTER_0_DSP_2_BASE_ADDR,
							   FE310_IRQTESTER_0_DSP_3_BASE_ADDR},
	.irqtester_cfg_func     = irqtester_fe310_cfg_0,
};

struct irqtester_driver_api{
	// dummy, we implement no api. But needed to properly init.
	// todo: implement api
};

static const struct irqtester_driver_api irqtester_fe310_driver = {
	// dummy, we implement no api. But needed to properly init.
};

#if CONFIG_FE310_IRQT_DRIVER
// ambigious how the 'name' works
DEVICE_AND_API_INIT(irqtester_fe310, CONFIG_IRQTESTER_FE310_NAME,
         irqtester_fe310_init,
		 &irqtester_fe310_data0, &irqtester_fe310_config0, 
         POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		 &irqtester_fe310_driver); 

// Set up ISR handlers
// macro places in _sw_isr_table, which is called by plic_fe310 driver
// Note: Interrupt caused by firing the irqtester device
// is always handled by own irqtester_fe310_irq_handler defined here.
// Applications can register a callback executed within the handler. 
static void irqtester_fe310_cfg_0(void)
{
	
	IRQ_CONNECT(FE310_IRQTESTER_0_IRQ_0,
		    CONFIG_IRQTESTER_FE310_0_PRIORITY,
		    irqtester_fe310_irq_handler,
		    DEVICE_GET(irqtester_fe310),
		    0);
	IRQ_CONNECT(FE310_IRQTESTER_0_IRQ_1,
		    CONFIG_IRQTESTER_FE310_0_PRIORITY,
		    irqtester_fe310_irq_handler,
		    DEVICE_GET(irqtester_fe310),
		    0);
	IRQ_CONNECT(FE310_IRQTESTER_0_IRQ_2,
		    CONFIG_IRQTESTER_FE310_0_PRIORITY,
		    irqtester_fe310_irq_handler,
		    DEVICE_GET(irqtester_fe310),
		    0);

	#if CONFIG_FE310_ISR_PLIC_OPT_LVL > 0
	// no need to place in _sw_isr_table
	plic_fe310_connect_handler_fast(plic_fast_handler);
	#endif

}
#endif // CONFIG_FE310_IRQT_DRIVER

/**@endcond*/