/*
 *  Based on gpio_fe310 driver by Jean-Paul Etienne <fractalclone@gmail.com>
 */

#include "irqtestperipheral.h"
#include <soc.h>
#include <misc/printk.h> // debug only, 
#define SYS_LOG_DOMAIN "IrqTestPeripheral"  // todo: fix not working atm
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
#define CONFIG_IRQTESTER_FE310_0_PRIORITY 	1
#define CONFIG_IRQTESTER_FE310_NAME 		"irqtester0"


/*
 * Structs a driver has as expected by the kernel 
 * -----------------------------------------------------
 */

typedef void (*fe310_cfg_func_t)(void);

/* fe310 irqtest peripheral register-set structure */
struct irqtester_fe310_t {
	unsigned int value;
	unsigned int perval;
	unsigned int status;
	bool fire;
	bool enable;
};

struct irqtester_fe310_config {
	u32_t               irqtester_base_addr;
	fe310_cfg_func_t    irqtester_cfg_func; // called at device init, IRQ registering
};

// atomic flags used for sync, reffering to _data.flags
enum flags{
	IRQT_QUEUE_RX_ENABLED,
};

// data tied to a instance of the driver
struct irqtester_fe310_data {
	/* callback function 
	 * atm simple void to void function	
	 */
	void (*cb)(void);
	atomic_t flags;
	// use internally only
	struct device * _drv_handle; 
	struct k_msgq * _queue_rx;
	bool _queue_enabled;
};

static struct irqtester_fe310_data irqtester_fe310_data0;  // empty data init


// Internal helper macros to get handles
#define DEV_CFG(dev)						\
	((const struct irqtester_fe310_config * const)(dev)->config->config_info)
#define DEV_REGS(dev)							\
	((volatile struct irqtester_fe310_t *)(DEV_CFG(dev))->irqtester_base_addr)
#define DEV_DATA(dev)				\
	((struct irqtester_fe310_data *)(dev)->driver_data)
#define DEV()	\
	irqtester_fe310_data0._drv_handle	// call after init only
#define LEN_ARRAY(arr) \
	((sizeof arr) / (sizeof *arr))		// call only on arrays defined in scope


/*
 * Definition of internal variables to store and manage values.
 * Use get_value() to access!
 * - WARNING: Implementing as static variable means the driver must
 * be instantiated only once! Move to driver_data if needed.
 * - Auto-generated. Take care when manually adding values to keep
 * val_name enum from header in sync with _values_<..> arrays.
 */


// data pool for int like data
static struct DrvValue_int _values_int[] = {
	{.base.id_name = VAL_IRQ_0_PERVAL},
};

// data pool for bool like data
static struct DrvValue_int _values_bool[] = {
	{.base.id_name = VAL_IRQ_0_ENABLE},
};



/*
 * IRQ Handler registered to the kernel, just calls a callback.
 * 
 * @param arg: When kernel calls, this is a handle to the device.
 */

static void irqtester_fe310_irq_handler(void *arg){
	//printk("IRQTester interrupt triggered\n");

	// get handles
	struct device *dev = (struct device *)arg;
	struct irqtester_fe310_data *data = DEV_DATA(dev);
	if(data->cb != NULL){
		// fire callback
		data->cb();
	}
}

/*
 * ISRs to pass data upwards.
 * 
 * -----------------------------------------------------
 */

static void _send_event_2_queue_rx(struct device * dev, struct DrvEvent * evt){
	struct irqtester_fe310_data * data = DEV_DATA(dev);
	bool send_fail = false;
	if(atomic_test_bit( &(data->flags), IRQT_QUEUE_RX_ENABLED )){

		if(0 != k_msgq_put(data->_queue_rx, evt, K_NO_WAIT))
			send_fail = true;
		return; // success, so get out of here fast
	}
	if(send_fail){
		SYS_LOG_WRN("Couldn't send event with id %i to busy, full or disabled queue", evt->id_name);
	}
}

// handler for very block with irq
static void _irq_0_handler(void){
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
	_values_int[id_2_index(VAL_IRQ_0_PERVAL)].payload = perval;
	_values_int[id_2_index(VAL_IRQ_0_PERVAL)].base.time_ns = now_ns; 

	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].payload = enable;
	_values_bool[id_2_index(VAL_IRQ_0_ENABLE)].base.time_ns = now_ns;

	// issue events to queue
	struct DrvEvent evt_perval = {.id_name=VAL_IRQ_0_PERVAL, .val_type=DRV_INT, .event_type=VAL_UPDATE};
	struct DrvEvent evt_enable = {.id_name=VAL_IRQ_0_ENABLE, .val_type=DRV_BOOL, .event_type=VAL_UPDATE};
	_send_event_2_queue_rx(dev, &evt_perval);
	_send_event_2_queue_rx(dev, &evt_enable);

}


/*
 * Public function available to applications. 
 * -----------------------------------------------------
 */

// todo: public functions should probably have a device param to differentiate between instances

// thread safe getter
// warning: be sure that res is a pointer to a DrvValue_ struct
// of correct type. We cast unsafely here.
int irqtester_fe310_get_val(irqt_val_t id, void * res){
	// todo: obtain semaphore to be thread safe

	short mode = 0;
	int n_int = LEN_ARRAY(_values_int);
	int m_bool = LEN_ARRAY(_values_bool);
	if(id < n_int)
		mode = 1; // int
	else if(id >= n_int && id < (n_int + m_bool))
		mode = 2; // bool
	else{
		SYS_LOG_WRN("Tried to get value with unknown id %i", id);
		return 1;
	}
	/* enter CRITICAL SECTION
	 * interrupts write to internal value pools
	 * we must not be interrupted while reading these 
	 */ 
	unsigned int lock_key = irq_lock();

	if (mode == 1){
		((struct DrvValue_int *) res)->base.id_name = id;
		((struct DrvValue_int *) res)->payload = _values_int[id_2_index(id)].payload;
		((struct DrvValue_int *) res)->base.time_ns = _values_int[id_2_index(id)].base.time_ns;
	}
	if (mode == 2){
		((struct DrvValue_bool *) res)->base.id_name = id;
		((struct DrvValue_bool *) res)->payload = _values_bool[id_2_index(id)].payload;
		((struct DrvValue_bool *) res)->base.time_ns = _values_bool[id_2_index(id)].base.time_ns;
	}

	irq_unlock(lock_key);

	return 0;
		 
}

/*
 * Provide a handle to a queue to receive msgs from the driver there.
 * Remember to enable_queue_rx() and to empty it in the consuming thread. 
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

int irqtester_fe310_enable_queue_rx(struct device * dev){
	struct irqtester_fe310_data *data = DEV_DATA(dev);

	if(data->_queue_rx == NULL){
		SYS_LOG_WRN("No queue to enable");
		return 1;
	}
	atomic_set_bit(&(data->flags), IRQT_QUEUE_RX_ENABLED);

	return 0;
}

void irqtester_fe310_dbgprint_event(struct device * dev, struct DrvEvent * evt){

	irqt_val_t id = evt->id_name;

	SYS_LOG_DBG("Driver event [type / value type]: %i, %i", evt->event_type, evt->val_type);
	// todo: check whether (implicit) casting like this is save!
	if(evt->val_type == DRV_INT){
		struct DrvValue_int val;
		irqtester_fe310_get_val(id, &val);
		SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val.base.id_name, val.payload, val.base.time_ns);
	} 
	else if(evt->val_type == DRV_BOOL){
		struct DrvValue_bool val;
		irqtester_fe310_get_val(id, &val);
		SYS_LOG_DBG("Value (id: %i): %i updated at %u ns", val.base.id_name, val.payload, val.base.time_ns);
	}
	else
		SYS_LOG_WRN("Can't print event of with unknown value type %i", evt->val_type);
}

/*
 * Internal driver functions. 
 * First argument must always be handle to driver.
 * -----------------------------------------------------
 */

// TODO: encapsulate hardware reads, make functions static

int id_2_index(irqt_val_t id){

	int n_int = LEN_ARRAY(_values_int);
	int m_bool = LEN_ARRAY(_values_bool);
	if(id < n_int)
		return id; // int
	else if(id >= n_int && id < (n_int + m_bool))
		return id - n_int; // bool
	else{
		SYS_LOG_ERR("Tried to get index for unknown id %i", id);
		return -1;
	}
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

	SYS_LOG_DBG("Init iqrtester driver with hw at mem address %p", irqt);

	/* Ensure that all registers are reset to 0 initially */
	irqt->enable	= 0;
	irqt->value		= 0;
	irqt->perval	= 0;
	irqt->status	= 0;
	irqt->fire   	= 0;

	/* Setup IRQ handler  */
	// Strange that not done by kernel...
	cfg->irqtester_cfg_func();
	// default handler 
	irqtester_fe310_register_callback(dev, _irq_0_handler);

	// store handle to driver in data for internal calls
	struct irqtester_fe310_data *data = DEV_DATA(dev);
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

int irqtester_fe310_get_perval(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->perval;
	//printk("Getting perval %i from %p \n", *res, &(irqt->perval));

	return 0;
}

int irqtester_fe310_get_status(struct device *dev, unsigned int * res)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	*res = irqt->status;
	//printk("Getting perval %i from %p \n", *res, &(irqt->perval));

	return 0;
}

int irqtester_fe310_enable(struct device *dev)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->enable = 1;
	//printk("Enabling by writing 1 to %p \n", &(irqt->enable));
    //printk("enable: %i \n", irqt->enable);
	return 0;
}

int irqtester_fe310_disable(struct device *dev)
{	
	volatile struct irqtester_fe310_t *irqt = DEV_REGS(dev);

	irqt->enable = 0;

	return 0;
}

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
 * ---------------------------------------------------------------
 */

static void irqtester_fe310_cfg_0(void); // forward declared

static const struct irqtester_fe310_config irqtester_fe310_config0 = {
	.irqtester_base_addr    = FE310_IRQTESTER_0_BASE_ADDR,
	.irqtester_cfg_func     = irqtester_fe310_cfg_0,
};

struct irqtester_driver_api{
	// dummy, we implement no api. But needed to properly init.
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