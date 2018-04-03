/**
 * @file
 * @brief low-level driver functions to interact with hardware
 */


#ifndef IRQTESTPERIPHERAL_H
#define IRQTESTPERIPHERAL_H

#include <device.h>

// todo: 
// - refine naming convention for all global namespace items
//   st. values and functions visibale to the outside
//   are unique in namespace and share leading identifier eg. IRQT_tet


/** List all available values with naming convention
 * VAL_<ORIGIN_BLOCK>_<NAME>
 */
typedef enum{
    // order must match .c _values_ arrays!
    _NIL_VAL            /// not a value, must be first
    /// start uints
    ,VAL_IRQ_0_PERVAL, 
    VAL_IRQ_0_VALUE,    // input
    VAL_IRQ_0_STATUS,  
    VAL_IRQ_1_STATUS,
    VAL_IRQ_1_PERIOD,   // input
    VAL_IRQ_1_NUM_REP,  // input
    VAL_IRQ_2_STATUS, 
    VAL_IRQ_2_PERIOD,   // input
    VAL_IRQ_2_NUM_REP,  // input
    VAL_DSP_3_STATUS,
    VAL_DSP_3_PERVAL,
    VAL_DSP_3_ID_COUNTER,
    VAL_DSP_3_VALUE,    // input
    VAL_DSP_3_PERIOD,   // input
    VAL_DSP_3_DUTY,     // input
    VAL_DSP_3_NUM_REP,  // input
    VAL_DSP_3_DELAY,    // input
    VAL_DSP_3_CLEAR_ID, // input
    /// start bools
    VAL_IRQ_0_ENABLE,   // input
    VAL_IRQ_1_CLEAR,    // input
    VAL_IRQ_2_CLEAR,    // input
    VAL_DSP_3_READY,
    VAL_DSP_3_RESET,    // input
    _NUM_VALS           /// must be last
}irqt_val_id_t;

/** 
 * List all available event types when receiving a DrvEvent
 */
typedef enum{
    EVT_T_VAL_UPDATE,   
	EVT_T_IRQ    
}irqt_event_type_t;

/**
 *  List all available value types when receiving a DrvValue
 */
typedef enum{
    _NIL_VAL_TYPE,
    VAL_T_UINT,
    VAL_T_INT,   
    VAL_T_BOOL,
    _NUM_VAL_TYPES
}irqt_val_type_t;

/**
 *  List all IRQ sources when receiving a DrvValue
 */
typedef enum{
    IRQ_0,
    IRQ_1,
    IRQ_2,
    _NUM_IRQS   /// must be last
}irqt_irq_id_t;

/**
 *  Driver Event
 */
struct DrvEvent{
    void * _reserved;   // for sending through fifo
    short cleared;      // how often read, necesary for sem + arr
    irqt_val_id_t val_id; 
    irqt_val_type_t val_type;
    irqt_event_type_t event_type;
    irqt_irq_id_t irq_id;
    short prio;         /// higher values have higher prio, for same prio later event is preferred
};

/** Driver Value. Generic parent struct to be casted to 
 *  correct subtype. This can be inferred by .val_type in 
 *  a DrvEvent.
 */
struct DrvValue_gen{
    irqt_val_id_t id_name;
    u32_t time_cyc; 
};

// "inherits" from generic DrvValue_gen
struct DrvValue_int{
    struct DrvValue_gen _super;   
    volatile int * base_addr;
    int payload;
};
struct DrvValue_uint{
    struct DrvValue_gen _super;
    volatile u32_t * base_addr;
    u32_t payload;
};
struct DrvValue_bool{
    struct DrvValue_gen _super;
    volatile bool * base_addr;
    bool payload;
};


// todo: clearify which functions need 'struct device * dev' param

int irqtester_fe310_reset_hw(struct device *dev);

int irqtester_fe310_get_val(irqt_val_id_t id_name, void * res);
int irqtester_fe310_get_val_uint(irqt_val_id_t id, void * res_value);
u32_t irqtester_fe310_get_val_uint_raw(irqt_val_id_t id);
int irqtester_fe310_get_reg(struct device * dev, irqt_val_id_t id, void * res_val);

int irqtester_fe310_set_reg(struct device * dev, irqt_val_id_t id, void * set_value);
int irqtester_fe310_set_reg_fast(struct device * dev, irqt_val_id_t id, void * set_val);
int irqtester_fe310_set_reg_uint_fast(struct device * dev, irqt_val_id_t id, void * set_val);

int irqtester_fe310_register_queue_rx(struct device * dev, struct k_msgq * queue);
int irqtester_fe310_register_fifo_rx(struct device * dev, struct k_fifo * queue);
int irqtester_fe310_register_sem_arr_rx(struct device * dev, struct k_sem * sem, struct DrvEvent evt_arr[], int len);

int irqtester_fe310_enable_queue_rx(struct device * dev);
int irqtester_fe310_enable_valflags_rx(struct device * dev);
int irqtester_fe310_enable_fifo_rx(struct device * dev);
int irqtester_fe310_enable_sem_arr_rx(struct device * dev);

int irqtester_fe310_receive_evt_from_arr(struct device * dev, struct DrvEvent * res, s32_t timeout);

int irqtester_fe310_purge_rx(struct device * dev);

bool irqtester_fe310_test_valflag(struct device *dev, irqt_val_id_t id_name);
void irqtester_fe310_clear_all_valflags(struct device *dev);

void irqtester_fe310_dbgprint_event(struct device * dev, struct DrvEvent * evt);

int irqtester_fe310_fire(struct device *dev);
void irqtester_fe310_fire_1(struct device *dev);
int irqtester_fe310_fire_2(struct device *dev);
void irqtester_fe310_clear_1(struct device *dev);
void irqtester_fe310_clear_2(struct device *dev);

// semi private
int irqtester_fe310_register_callback(struct device *dev, irqt_irq_id_t irq_id, void (*cb)(void));
int irqtester_fe310_unregister_callback(struct device *dev, irqt_irq_id_t irq_id);




// todo: make private
/*
int irqtester_fe310_set_value(struct device *dev, unsigned int val);



int irqtester_fe310_get_value(struct device *dev, unsigned int * res);

int irqtester_fe310_get_status(struct device *dev, unsigned int * res);
*/
int irqtester_fe310_enable(struct device *dev);
int irqtester_fe310_get_perval(struct device *dev, unsigned int * res);
int irqtester_fe310_get_enable(struct device *dev, bool * res);
int irqtester_fe310_disable(struct device *dev);


// for tests only, todo: make static inline
void _irq_gen_handler(void);

void _irq_0_handler_1(void);
void _irq_0_handler_2(void);
void _irq_0_handler_3(void);
void _irq_0_handler_4(void);
void _irq_0_handler_5(void);
void _irq_0_handler_6(void);

void _irq_1_handler_0(void);
void _irq_1_handler_1(void);

void _irq_2_handler_0(void);
void _irq_2_handler_1(void);


#endif