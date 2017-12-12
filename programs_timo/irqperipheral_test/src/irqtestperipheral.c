/** 
 *  @file
 *  Driver for IrqTestPeripheral hw revision 1.
 *  Singleton, only register one instance with kernel!
 *  Based on gpio_fe310 driver by Jean-Paul Etienne <fractalclone@gmail.com>
 */

// todo: 
// - public functions should probably have a device param to differentiate between instances
// - decide whether pure singleton or move static vars to data

#include "irqtestperipheral.h"
#include <soc.h>
#include <misc/printk.h> // debug only, 
#define SYS_LOG_DOMAIN "IrqTestPeripheral"  
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>
#include <atomic.h>

#define FE310_IRQTESTER_0_IRQ             	(RISCV_MAX_GENERIC_IRQ + 53)
#define FE310_IRQTESTER_0_BASE_ADDR		  	0x2000
#define FE310_IRQTESTER_0_STATUS_BLOCKED   	2
/* not needed offsets
#define FE310_IRQTESTER_0_VALUE_OFFSET		0x00
#define FE310_IRQTESTER_0_PERVAL_OFFSET		0x04
#define FE310_IRQTESTER_0_STATUS_OFFSET		0x08
#define FE310_IRQTESTER_0_FIRE_OFFSET		0x0C
#define FE310_IRQTESTER_0_ENABLE_OFFSET		0x0D
*/
// below should actually be set by kconfig and end up in auoconf.h
// todo: move to kconfig system
#define CONFIG_IRQTESTER_FE310_0_PRIORITY 	1
#define CONFIG_IRQTESTER_FE310_NAME 		"irqtester0"

#ifdef TEST_MINIMAL
#warning "Building minimal version of driver"
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
#define DEV_REGS(dev)							\
	((volatile struct irqtester_fe310_t *)(DEV_CFG(dev))->irqtester_base_addr)
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
};

// data pool for int like data
static struct DrvValue_int _values_int[] = {
	
};

// data pool for bool like data
static struct DrvValue_bool _values_bool[] = {
	{._super.id_name = VAL_IRQ_0_ENABLE},
};

#define NUM_VALS() \
	(LEN_ARRAY(_values_uint) + LEN_ARRAY(_values_int) +  LEN_ARRAY(_values_bool))


// forward declare static, internal helper functions
static inline irqt_val_type_t id_2_type(irqt_val_id_t id);
static inline int id_2_index(irqt_val_id_t id);
static inline bool test_flag(struct device * dev, int drv_flag);
static bool test_any_send_flag(struct device * dev);

/*
 * Structs a driver has as expected by the kernel 
 * ----------------------------------------------------------------------------
 * @cond PRIVATE
 */

typedef void (*fe310_cfg_func_t)(void);

/// fe310 irqtest peripheral register-set structure 
struct irqtester_fe310_t {
	unsigned int value;
	unsigned int perval;
	unsigned int status;
	bool fire;
	bool enable;
};

/// config struct for registering with kernel
struct irqtester_fe310_config {
	u32_t               irqtester_base_addr;
	fe310_cfg_func_t    irqtester_cfg_func; // called at device init, IRQ registering
};

/// data tied to a instance of the driver
struct irqtester_fe310_data {
	/* callback function 
	 * atm simple void to void function	
	 */
	void (*cb)(void);
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

	// get handles
	struct device *dev = (struct device *)arg;
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	if(data->cb != NULL){
		// fire callback
		data->cb();
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
		else
			return; // success, so get out of here fast
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


// handlers for every block with irq
// note: in zephyr all ISRs have same priority by default
// -> ISR can't be preempted by other ISR
// todo: optimize isr parts, avoid non inline function calls
// make getters inline
// check cpu register usage -> should run on register mem only
// make static
void _irq_0_handler(void){

	/* todo: for new hardware revision: 
	 * - delayed fire / hw timers
	 * - signal irq cleared to hw
	 */ 
	// todo: - check all time_ns code to be safe for high reliability

	/* generic part */
	struct device * dev = DEV();
	if(dev == NULL)
		return;	// safety first

	

	/* own implementation 
	 * read values from hardware registers and send up DrvEvents
	 */
	int perval;
	bool enable;
	irqtester_fe310_get_perval(dev, &perval);
	irqtester_fe310_get_enable(dev, &enable);
	

	// write into internal data pools 
	u32_t now_ns = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_ns = now_ns; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_ns = now_ns;

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=DRV_INT, .event_type=VAL_UPDATE, .irq_id=IRQ_0};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=DRV_BOOL, .event_type=VAL_UPDATE, .irq_id=IRQ_0};
	struct DrvEvent evt_irq0   = {.val_id=_NIL_VAL, .event_type=IRQ, .irq_id=IRQ_0};

	// inefficient to test bit here, but this is a default ISR for testing only
	if(test_any_send_flag(dev)){
		send_event_rx(dev, &evt_irq0);
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
	struct DrvValue_uint perval;
	struct DrvValue_bool enable;
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &perval);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	

	// write into internal data pools 
	u32_t now_ns = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval.payload;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_ns = now_ns; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable.payload;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_ns = now_ns;

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=DRV_INT, .event_type=VAL_UPDATE};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=DRV_BOOL, .event_type=VAL_UPDATE};

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
	struct DrvValue_uint perval;
	struct DrvValue_bool enable;
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_PERVAL, &perval);
	irqtester_fe310_get_reg(dev, VAL_IRQ_0_ENABLE, &enable);
	

	/* write into internal data pools */
	u32_t now_ns = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32()); // might be written into event
	/*
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval.payload;
	_values_uint[id_2_index(VAL_IRQ_0_PERVAL)]._super.time_ns = now_ns; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable.payload;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)]._super.time_ns = now_ns;
	*/

	// issue events to queue
	struct DrvEvent evt_perval = {.val_id=VAL_IRQ_0_PERVAL, .val_type=DRV_INT, .event_type=VAL_UPDATE};
	struct DrvEvent evt_enable = {.val_id=VAL_IRQ_0_ENABLE, .val_type=DRV_BOOL, .event_type=VAL_UPDATE};

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
	int perval;
	irqtester_fe310_get_perval(dev, &perval);
	
	// write into internal data pools 
	// no timing support
	_values_uint[VAL_IRQ_0_PERVAL - 1].payload = perval; // only works for uint type values
	struct DrvEvent evt_irq0   = {.val_id=_NIL_VAL, .event_type=IRQ, .irq_id=IRQ_0};

	// manually inlining send, flag functions
	k_msgq_put(data->_queue_rx, &evt_irq0, K_NO_WAIT);
	//atomic_set_bit(data->_valflags_rx, VAL_IRQ_0_PERVAL);
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

	irqt_val_type_t type = id_2_type(id);

	//SYS_LOG_DBG("Getting value for id %i, type %i", id, type);

	/* enter CRITICAL SECTION
	 * ISRs write to internal value pools
	 * we must not be interrupted while reading these 
	 */ 
	unsigned int lock_key = irq_lock();

	switch(type){
		case DRV_UINT:;
			//struct DrvValue_uint * dbg = ((struct DrvValue_uint *) res_value);
			((struct DrvValue_uint *) res_value)->_super.id_name = id;
			((struct DrvValue_uint *) res_value)->payload = _values_uint[id_2_index(id)].payload;
			((struct DrvValue_uint *) res_value)->_super.time_ns = _values_uint[id_2_index(id)]._super.time_ns;
			//SYS_LOG_DBG("at %p payload %i, time %i", dbg, dbg->payload, dbg->_super.time_ns);
			break;
		case DRV_INT:
			((struct DrvValue_int *) res_value)->_super.id_name = id;
			((struct DrvValue_int *) res_value)->payload = _values_int[id_2_index(id)].payload;
			((struct DrvValue_int *) res_value)->_super.time_ns = _values_int[id_2_index(id)]._super.time_ns;
			break;
		case DRV_BOOL:
			((struct DrvValue_bool *) res_value)->_super.id_name = id;
			((struct DrvValue_bool *) res_value)->payload = _values_bool[id_2_index(id)].payload;
			((struct DrvValue_bool *) res_value)->_super.time_ns = _values_bool[id_2_index(id)]._super.time_ns;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
	}	

	irq_unlock(lock_key);

	return 0;
		 
}

/** 
 * @brief Thread safe generic setter for device registers.
 * Must not be called by an ISR (mutex).
 * @param id: id of value to set
 * @param set_value: pointer to a DrvValue_X struct of correct type. 
 */ 
int irqtester_fe310_set_reg(struct device * dev, irqt_val_id_t id, void * set_val){
	
	// only one thread at a time can write to reg
	// todo: test mutex lock
	static struct k_mutex lock;
	k_mutex_init(&lock);

	irqt_val_type_t type = id_2_type(id);

	if (k_mutex_lock(&lock, K_MSEC(1)) != 0) {
		SYS_LOG_WRN("Couldn't obtain lock to set registers with id %i", id);
		return 1;
	}

	/* enter CRITICAL SECTION
	 * we must not be interrupted while setting these 
	 */ 

	unsigned int lock_key = irq_lock();

	// cast the given value container depending on 
	// its type to set the value in corresponding memory pool
	switch(type){
		case DRV_UINT: 
			*(_values_uint[id_2_index(id)].base_addr) = \
				((struct DrvValue_uint *)set_val)->payload;
			break;
		case DRV_INT: 
			*(_values_int[id_2_index(id)].base_addr) = \
				((struct DrvValue_int *)set_val)->payload;
			break;
		case DRV_BOOL: 
			*(_values_bool[id_2_index(id)].base_addr) = \
				((struct DrvValue_bool *)set_val)->payload;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
	}
	
	irq_unlock(lock_key);
	k_mutex_unlock(&lock);


	return 0;
}

/**
 * @brief Non-thread safe, generic getter for device registers.
 * 
 * Might be called from an ISR, but slower than direct getters.
 * @param id: id of value to set
 * @param set_value: pointer to a DrvValue_X struct of correct type. 
 */ 
int irqtester_fe310_get_reg(struct device * dev, irqt_val_id_t id, void * res_val){
	
	irqt_val_type_t type = id_2_type(id);
	u32_t now_ns = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());

	/* enter CRITICAL SECTION
	 * we must not be interrupted while getting these 
	 */ 
	
	unsigned int lock_key = irq_lock();
	
	// cast the given value container depending on its type 
	switch(type){
		case DRV_UINT:
			((struct DrvValue_uint *) res_val)->_super.id_name = id;
			((struct DrvValue_uint *) res_val)->payload = *(_values_uint[id_2_index(id)].base_addr);
			((struct DrvValue_uint *) res_val)->_super.time_ns = now_ns;
			break;
		case DRV_INT:
			((struct DrvValue_int *) res_val)->_super.id_name = id;
			((struct DrvValue_int *) res_val)->payload = *(_values_int[id_2_index(id)].base_addr);
			((struct DrvValue_int *) res_val)->_super.time_ns = now_ns;
			break;
		case DRV_BOOL:
			((struct DrvValue_bool *) res_val)->_super.id_name = id;
			((struct DrvValue_bool *) res_val)->payload = *(_values_bool[id_2_index(id)].base_addr);
			((struct DrvValue_bool *) res_val)->_super.time_ns = now_ns;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
	}
	
	irq_unlock(lock_key);


	return 0;

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
	
	if(res->cleared != 0)
		SYS_LOG_WRN("Event with id %i at count %i shouln't be cleared yet!", res->val_id, count);
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
	
	if(evt->event_type == VAL_UPDATE){
		if(evt->val_type == DRV_INT){
			struct DrvValue_int val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val._super.id_name, val.payload, val._super.time_ns);
		} 
		else if(evt->val_type == DRV_UINT){
			struct DrvValue_uint val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %u updated at %u ns", val._super.id_name, val.payload, val._super.time_ns);
		} 
		else if(evt->val_type == DRV_BOOL){
			struct DrvValue_bool val;
			irqtester_fe310_get_val(id, &val);
			SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val._super.id_name, val.payload, val._super.time_ns);
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

// calc (enum) type from value id 
static inline irqt_val_type_t id_2_type(irqt_val_id_t id){

	int n_uint = LEN_ARRAY(_values_uint);
	int m_int = LEN_ARRAY(_values_int);
	int k_bool = LEN_ARRAY(_values_bool);

	if(id > 0 && (id < n_uint + 1)){	// +1 -> id 0=_NIL_VAL
		//SYS_LOG_DBG("Returning type %i", DRV_UINT);
		return DRV_UINT;
	} 
	else if(id >= n_uint + 1 && id < (n_uint + m_int + 1)){
		//SYS_LOG_DBG("Returning type %i", DRV_INT);
		return DRV_INT;
	}
	else if(id >= (n_uint + m_int + 1)  && id < (n_uint + m_int + k_bool + 1)){
		//SYS_LOG_DBG("Returning type %i", DRV_BOOL);
		return DRV_BOOL;
	}
	else{
		SYS_LOG_ERR("Tried to get type for unknown id %i", id);
		return -1;
	}
}

// calc index to access the _values_ arrays from value id
// order of _values_id must match .h::irqt_val_id_t
// todo: in ISR, this is performance critical
// check assembly whether compiler optimizes
// if not: make macro, since all values are known at compile time
static inline int id_2_index(irqt_val_id_t id){

	int n_uint = LEN_ARRAY(_values_uint);
	int m_int = LEN_ARRAY(_values_int);
	//int k_bool = LEN_ARRAY(_values_bool);

	irqt_val_type_t type = id_2_type(id);

	switch(type){
		// +1 -> id 0=_NIL_VAL
		case DRV_UINT:	
			//SYS_LOG_DBG("Returning idx %i", id);
			return id - 1; // uint
		case DRV_INT:
			//SYS_LOG_DBG("Returning idx %i", id - n_uint);
			return id - n_uint - 1; // int
		case DRV_BOOL:
			//SYS_LOG_DBG("Returning idx %i", id - n_uint - m_int);
			return id - n_uint - m_int - 1; // bool
		default:
			SYS_LOG_ERR("Tried to get index for unknown id %i", id);
			return -1;
	}
}


// helper
static inline bool test_flag(struct device * dev, int drv_flag){
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	return atomic_test_bit( &(data->flags), drv_flag);
}

static int _store_reg_addr(irqt_val_id_t id, volatile void * addr){
	
	irqt_val_type_t type = id_2_type(id);
	SYS_LOG_DBG("Saving addr for value id %i: %p", id, addr);

	switch(type){
		case DRV_UINT:
			_values_uint[id_2_index(id)].base_addr = (u32_t *)addr;
			break;
		case DRV_INT:
			_values_int[id_2_index(id)].base_addr = (int *)addr;
			break;
		case DRV_BOOL:
			_values_bool[id_2_index(id)].base_addr = (bool *)addr;
			break;
		default:
			SYS_LOG_ERR("Unknown type %i", type);
			return 1;
	}
	
	return 0;
}

/**
 * @brief Initialize 
 * @param dev IRQTester device struct
 *
 * @return 0
 */
static int irqtester_fe310_init(struct device *dev)
{
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);
	const struct irqtester_fe310_config *cfg = DEV_CFG(dev);
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	SYS_LOG_DBG("Init iqrtester driver with hw at mem address %p", irqt);

	/* Ensure that all registers are reset to 0 initially */
	irqt->enable	= 0;
	irqt->value		= 0;
	irqt->perval	= 0;
	irqt->status	= 0;
	irqt->fire   	= 0;
	// store addr of registers to allow set/get by id_name
	_store_reg_addr(VAL_IRQ_0_ENABLE, 	&(irqt->enable)	);
	_store_reg_addr(VAL_IRQ_0_VALUE, 	&(irqt->value)	);
	_store_reg_addr(VAL_IRQ_0_PERVAL, 	&(irqt->perval)	);

	/* Setup IRQ handler  */
	// Strange that not done by kernel...
	cfg->irqtester_cfg_func();
	// default handler 
	#ifndef TEST_MINIMAL
	irqtester_fe310_register_callback(dev, _irq_0_handler);
	#else
	irqtester_fe310_register_callback(dev, _irq_0_handler_5);
	#endif
	// store handle to driver in data for internal calls
	data->_drv_handle = dev;

	return 0;
}


int irqtester_fe310_register_callback(struct device *dev, void (*cb)(void))
{	

	// get handle to data and write cb into
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	data->cb = cb;

	/* Enable interrupt for the pin at PLIC level */
	irq_enable(FE310_IRQTESTER_0_IRQ);

	return 0;
}

int irqtester_fe310_unregister_callback(struct device *dev)
{	
	// get handle to data and write cb into
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	data->cb = NULL;

	/* Disable interrupt for the pin at PLIC level */
	irq_disable(FE310_IRQTESTER_0_IRQ);

	return 0;
}

/*
 * Non generic, non thread-safe setters for registers.
 * Avoid to call from application code.
 * ----------------------------------------------------------------------------
 */

int irqtester_fe310_set_value(struct device *dev, unsigned int val)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->value = val;
	//printk("Setting value %i to %p \n", val, &(irqt->value));
	//printk("value: %i \n", irqt->value);

	return 0;
}

int irqtester_fe310_get_value(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->value;
	//printk("Getting value %i from %p \n", *res, &(irqt->value));

	return 0;
}

int irqtester_fe310_get_enable(struct device *dev, bool * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->enable;

	return 0;
}

inline int irqtester_fe310_get_perval(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->perval;

	return 0;
}

int irqtester_fe310_get_status(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->status;

	return 0;
}

int irqtester_fe310_set_enable(struct device *dev, bool val)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->enable = val;

	return 0;
}

int irqtester_fe310_enable(struct device *dev)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->enable = 1;

	return 0;
}

int irqtester_fe310_disable(struct device *dev)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->enable = 0;

	return 0;
}

/**@endcond*/ 

int irqtester_fe310_fire(struct device *dev)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	// device goes to status blocked after fire
	// to indicate that fired once and blocked after
	irqt->fire = 1;
	int timeout = 100; //cpu cycles
	while(irqt->status != FE310_IRQTESTER_0_STATUS_BLOCKED || timeout < 0){
		// we (probably) can't k_wait() here, because may be called outside a thread
		timeout -= 1;
	}

	// rearm blocked (status 1) hardware
	irqt->fire = 0;

	if(timeout < 0){
		return 1;
		// hardware did not respond to fire command
	}

	timeout = 100;
	while(irqt->status != 0 || timeout < 0){
		timeout -= 1;
	}

	if(timeout < 0){
		return 2;
		// wan't able to unblock device
	}

	return 0;
}



/* 
 * Code below sets up and registers driver with kernel, no driver API used
 * @cond PRIVATE
 * ----------------------------------------------------------------------------
 */

static void irqtester_fe310_cfg_0(void); // forward declared

static const struct irqtester_fe310_config irqtester_fe310_config0 = {
	.irqtester_base_addr    = FE310_IRQTESTER_0_BASE_ADDR,
	.irqtester_cfg_func     = irqtester_fe310_cfg_0,
};

struct irqtester_driver_api{
	// dummy, we implement no api. But needed to properly init.
	// todo: implement api
};

static const struct irqtester_driver_api irqtester_fe310_driver = {
	// dummy, we implement no api. But needed to properly init.
};

// ambigious how the 'name' works
DEVICE_AND_API_INIT(irqtester_fe310, CONFIG_IRQTESTER_FE310_NAME,
         irqtester_fe310_init,
		 &irqtester_fe310_data0, &irqtester_fe310_config0, 
         POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		 &irqtester_fe310_driver); 

// Set up ISR handlers
// Note: Interrupt caused by firing the irqtester device
// is always handled by own irq_handler defined here.
// Applications can register a callback executed within the handler. 
static void irqtester_fe310_cfg_0(void)
{
	IRQ_CONNECT(FE310_IRQTESTER_0_IRQ,
		    CONFIG_IRQTESTER_FE310_0_PRIORITY,
		    irqtester_fe310_irq_handler,
		    DEVICE_GET(irqtester_fe310),
		    0);
}

/**@endcond*/