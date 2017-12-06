#ifndef STATES_H
#define STATES_H

#include <zephyr.h>
#include "irqtestperipheral.h"

#define STATES_REQ_VALS_MAX _NUM_VALS    // might need adjustment if many vals

/**
 * @file
 * @brief bundle all implementation details of different states
 */

typedef enum{
    CYCLE_STATE_IDLE,
    CYCLE_STATE_START, // aka preamble detect
    CYCLE_STATE_END,
    _NUM_CYCLE_STATES // must be last

}cycle_state_id_t;

typedef enum{
    _CYCLE_DEFAULT_EVT, // -> call next state as define by element 0 of transition table
    CYCLE_EVENT_RESET_IRQ,
    _NUM_CYCLE_EVENTS // must be last

}cycle_event_id_t;


struct State{
    cycle_state_id_t id_name;
    cycle_state_id_t default_next_state;
    u32_t timing_goal_start; // in cycles
    u32_t timing_goal_end;
    irqt_val_id_t val_ids_req[STATES_REQ_VALS_MAX];  // holds the irqt vals requested by state
    void (*action)(void);  // method called when switched to this state 
};


void states_configure(struct State * states, cycle_state_id_t * transition_table);
 


#endif