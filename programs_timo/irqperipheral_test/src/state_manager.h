/**
 * @file
 * @brief  Generic logic for running a state machine. Concrete implementation 
 *         happens elsewhere.
 */

#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <device.h>
#include "states.h"
#include "irqtestperipheral.h"


typedef enum{
    _NIL_RS,
    RS_WAIT_FOR_START,
    RS_WAIT_FOR_VAL,
    _NUM_WAIT_REASON
}sm_wait_reason_t;



// todo: all threadsafe, repeated call safe?

void state_mng_configure(struct State cust_states[], cycle_state_id_t * cust_tt, int len_states, int len_events);
void state_mng_init(struct device * dev);
void state_mng_print_state_config();
void state_mng_print_transition_table_config();
int state_mng_pull_perf_action(int res_buff[], int idx_buf, int len); 

bool state_mng_is_running();
cycle_state_id_t state_mng_get_current();
u8_t state_mng_get_current_subs();
struct State * state_mng_id_2_state(cycle_state_id_t id_name);

u32_t state_mng_get_time_delta();
int state_mng_get_timing_goal(struct State * state, u8_t substate, int mode);

void state_mng_run(void);   
int state_mng_start();
int state_mng_abort();

int state_mng_register_action(cycle_state_id_t state_id, void (*func)(struct ActionArg const *), irqt_val_id_t arr_vals[], int len);
int state_mng_purge_registered_actions(cycle_state_id_t state_id);
int state_mng_purge_registered_actions_all();

void state_mng_wait_fix(struct State * state, u32_t t_fix, sm_wait_reason_t reason);
bool state_mng_wait_vals_ready(struct State * state);

void state_mng_print_evt_log();

#endif