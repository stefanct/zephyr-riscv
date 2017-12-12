#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <device.h>
#include "states.h"
#include "irqtestperipheral.h"

// todo: all threadsafe, repeated call safe?
cycle_state_id_t state_mng_get_current();
void state_mng_configure(struct State cust_states[], cycle_state_id_t cust_tt[], int len_states, int len_events);
void state_mng_init(struct device * dev);
void state_mng_abort();
bool state_mng_is_running();
void state_mng_run(void);

int state_mng_register_action(cycle_state_id_t state_id, void (*func)(void), irqt_val_id_t arr_vals[], int len);
int state_mng_purge_registered_actions(cycle_state_id_t state_id);


#endif