/**
 * @file
 * @brief define state machine 'state' struct and configure to state_manager
 */


#ifndef STATES_H
#define STATES_H

#include <zephyr.h>
#include "irqtestperipheral.h"

#define STATES_REQ_VALS_MAX         5    // might need adjustment if many vals
#define STATES_CBS_PER_ACTION_MAX   5
//#define STATES_SUBSTATE_SER_DEPTH   3   
#define STATES_DIS_SUBSTATES 0      // 1 to deactivate substates  

/// declares all possible states (.id_names) of a state manager
typedef enum{
    _NIL_CYCLE_STATE,   // must be first
    CYCLE_STATE_IDLE,
    CYCLE_STATE_START, // aka preamble detect
    CYCLE_STATE_DL_CONFIG,
    CYCLE_STATE_DL,
    CYCLE_STATE_UL_CONFIG,
    CYCLE_STATE_UL,
    CYCLE_STATE_RL_CONFIG,
    CYCLE_STATE_RL,
    CYCLE_STATE_END,
    _NUM_CYCLE_STATES // must be last

}cycle_state_id_t;

/// declares all possible events (state_manager.c::SMEvent) causing transitions
typedef enum{
    _CYCLE_DEFAULT_EVT, // must be first -> call next state as define by element 0 of transition table
    CYCLE_EVENT_RESET_IRQ,
    _NUM_CYCLE_EVENTS // must be last

}cycle_event_id_t;


struct State{
    cycle_state_id_t id_name;
    cycle_state_id_t default_next_state;
    u32_t timing_goal_start; // in cpu cycles
    u32_t timing_goal_end;
    irqt_val_id_t val_ids_req[STATES_REQ_VALS_MAX];  // holds the irqt vals requested by state
    void (*action)(cycle_state_id_t);  // method called when switched to this state , note that usually _default_action()
    void (*handle_t_goal_start)(struct State *, int);  // method called when entering state, to act when time goal hit / missed    
    void (*handle_t_goal_end)(struct State *, int);    // method called when leaving state, to act when time goal hit / missed
    bool (*handle_val_rfail)(struct State *);    // called when check_vals_ready fails at beginning of state, returns true if actions should be skipped

    // serial substates logic: automatically invoked as next state
    // until all substates have occured (eg. for multiple user time slots)
    // they "inherit" all states var but modify timing goals
    // affects state_manager::switch_state() and ::check_time_goal()
    u8_t cur_subs_idx;
    u8_t max_subs_idx;
    u32_t timing_summand;  // added to timing_goals additionaly to duration of state
};

/// struct passed to any registered action as argument
struct ActionArg{
    struct State * state_cur;
};



void states_configure_auto(struct State * states, cycle_state_id_t * transition_table, void * action);
void states_configure_custom(struct State * states, cycle_state_id_t * transition_table, void * action, \
                                struct State cust_states[], cycle_state_id_t * cust_tt, int len_states, int len_events);
void states_configure_substates(struct State * state, u8_t num_substates, u8_t timing_summand);

int states_get_timing_goal_start(struct State * state, u8_t substate);
int states_get_timing_goal_end(struct State * state, u8_t substate);

void states_print_state(struct State * state);

#endif