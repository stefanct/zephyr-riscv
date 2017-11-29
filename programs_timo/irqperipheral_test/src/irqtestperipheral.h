#ifndef IRQTESTPERIPHERAL_H
#define IRQTESTPERIPHERAL_H

#include <device.h>


/* lists all available values with naming convention
 * VAL_<ORIGIN_BLOCK>_<NAME>
 */
typedef enum{
    VAL_IRQ_0_PERVAL,   // 0 
	VAL_IRQ_0_ENABLE    // 1 fake, actually input
}irqt_val_t;

typedef enum{
    VAL_UPDATE,   
	IRQ    
}irqt_event_type_t;

typedef enum{
    DRV_INT,   
	DRV_BOOL    
}irqt_val_type_t;


struct DrvEvent{
    irqt_val_t id_name;
    irqt_val_type_t val_type;
    irqt_event_type_t event_type;
};

// for casting any specific DrvValue
struct DrvValue_gen{
    irqt_val_t id_name;
    u32_t time_ns; // make sure SYS_CLOCK_HW_CYCLES_PER_SEC is correct
};

// "inherits" from generic DrvValue_gen
struct DrvValue_int{
    struct DrvValue_gen base;
    int payload;
};
struct DrvValue_bool{
    struct DrvValue_gen base;
    bool payload;
};


int irqtester_fe310_get_val(irqt_val_t id_name, void * res);
int irqtester_fe310_register_queue_rx(struct device * dev, struct k_msgq * queue);
int irqtester_fe310_enable_queue_rx(struct device * dev);
void irqtester_fe310_dbgprint_event(struct device * dev, struct DrvEvent * evt);


// semi private
int irqtester_fe310_register_callback(struct device *dev, void (*cb)(void));
int irqtester_fe310_unregister_callback(struct device *dev);


// todo: make private
int irqtester_fe310_set_value(struct device *dev, unsigned int val);
int irqtester_fe310_get_value(struct device *dev, unsigned int * res);
int irqtester_fe310_get_perval(struct device *dev, unsigned int * res);
int irqtester_fe310_get_status(struct device *dev, unsigned int * res);
int irqtester_fe310_get_enable(struct device *dev, bool * res);


int irqtester_fe310_enable(struct device *dev);
int irqtester_fe310_disable(struct device *dev);
int irqtester_fe310_fire(struct device *dev);




#endif