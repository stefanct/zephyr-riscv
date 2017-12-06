#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <device.h>
#include "states.h"

cycle_state_id_t state_mng_get_current();
void state_mng_init(struct device * dev);
void state_mng_abort();
cycle_state_id_t state_mng_get_current_state();
void state_mng_run(void);

#endif