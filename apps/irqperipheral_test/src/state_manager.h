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


struct Switch_Event{
    cycle_state_id_t from_state;
    u8_t from_substate;
    cycle_state_id_t to_state;
    u8_t to_substate;
    u32_t t_cyc;
    u32_t t_delta_cyc;  // time since sm reset
};

/// used for fast logging
struct Wait_Event{
    cycle_state_id_t state;
    u8_t substate;
    u32_t t_wait_cyc;
    sm_wait_reason_t reason;
};

/// user for measuring performance of actions
/// (Can use switch events, too.)
struct Perf_Event{
    cycle_state_id_t state;
    u8_t substate;
    u32_t t_actions_cyc;
};


// todo: all threadsafe, repeated call safe?

// control
void state_mng_configure(struct State cust_states[], cycle_state_id_t * cust_tt, int len_states, int len_events);
void state_mng_init(struct device * dev);
void state_mng_run(void);   
int state_mng_start();
int state_mng_abort();

// config
int state_mng_register_action(cycle_state_id_t state_id, void (*func)(struct ActionArg const *), irqt_val_id_t arr_vals[], int len);
int state_mng_purge_registered_actions(cycle_state_id_t state_id);
int state_mng_purge_registered_actions_all();

int state_mng_add_filter_state(cycle_state_id_t state_id);
int state_mng_purge_filter_state();
int state_mng_pull_perf_action(int res_buff[], int idx_buf, int len); 

int state_purge_switch_events();

// print
void state_mng_print_state_config();
void state_mng_print_transition_table_config();
void state_mng_print_evt_log();

// getters
cycle_state_id_t state_mng_get_current();
u8_t state_mng_get_current_subs();
int state_mng_get_switch_events(struct Switch_Event * buf, int len_buf);
u32_t state_mng_get_time_delta();
int state_mng_get_timing_goal(struct State * state, u8_t substate, int mode);
bool state_mng_is_running();

// helpers
struct State * state_mng_id_2_state(cycle_state_id_t id_name);
void state_mng_wait_fix(struct State * state, u32_t t_fix, sm_wait_reason_t reason);
bool state_mng_wait_vals_ready(struct State * state);


#endif