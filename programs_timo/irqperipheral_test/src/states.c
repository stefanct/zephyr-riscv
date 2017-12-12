#include "states.h"
#include "state_manager.h"

#define SYS_LOG_DOMAIN "States"  
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

/*
* Define states
* Use state_manager::register_action() to set additional
* state actions and required values.
*-----------------------------------------------------------------------------
*/

static struct State s_idle = {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State s_start = {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_END,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State s_end = {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0, 
        };

/**
 * @brief: Set up state array and transition table automatically. 
 * 
 * Uses states hard-coded in states.c
 * Registers a default action handler to every state, which is a function
 * that is responsible to fire all registered callbacks. 
 */
void states_configure_auto(struct State * states, cycle_state_id_t * transition_table, void * action){
    // non thread safe, never call if running
    // cast to pointers to array with correct dimensions
    struct State (*states_p)[_NUM_CYCLE_STATES]  = (struct State (*)[_NUM_CYCLE_STATES])states;
    cycle_state_id_t (*tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) transition_table;

    // set (usually default) action handler, argument is casted to correct function type 
    s_idle.action =     (void (*)(cycle_state_id_t))action;
    s_start.action =    (void (*)(cycle_state_id_t))action;
    s_end.action =      (void (*)(cycle_state_id_t))action;

    // initialize array for safety
    for(int i=0; i<STATES_REQ_VALS_MAX; i++){
        s_idle.val_ids_req[i] = _NIL_VAL;
        s_start.val_ids_req[i] = _NIL_VAL;
        s_end.val_ids_req[i] = _NIL_VAL;
    }

     // manually set up all states[_NUM_CYCLE_STATES] as defined in enum cycle_state_id_t
    (*states_p)[0] = s_idle;
    (*states_p)[1] = s_start;
    (*states_p)[2] = s_end;

    // manually set up transition table[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]
    // for CYCLE_STATE_IDLE
    cycle_event_id_t t0[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[0].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[0][0] = t0[0];   
    (*tt_p)[0][1] = t0[1];  
    // for CYCLE_STATE_START    
    cycle_event_id_t t1[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[1].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[1][0] = t1[0];   
    (*tt_p)[1][1] = t1[1];   
    // for CYCLE_STATE_END        
    cycle_event_id_t t2[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[2].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[2][0] = t2[0];   
    (*tt_p)[2][1] = t2[1];   

}

/**
 * @brief: Set up state array and transition table of state manager by customnly deployed arrays. 
 * 
 * Registers a default action handler to every state, which is a function
 * that is responsible to fire all registered callbacks. 
 * 
 * @param states:           Pointer to state_manager state array.
 * @param transition_table: Pointer to state_manager transition_table.
 * @param action:           Function pointer (void-2-void) to default action, usually state manager action dispatcher.
 * @param cust_states[]:    All states to be configured to state machine. 
 * @param cust_tt[]:        Transition table to be configure to state machine.
 */
void states_configure_custom(struct State * states, cycle_state_id_t * transition_table, void * action, \
                                struct State cust_states[], cycle_state_id_t cust_tt[], int len_states, int len_events){
    // non thread safe, never call if running
    // cast to pointers to array with correct dimensions
    struct State (*states_p)[_NUM_CYCLE_STATES]  = (struct State (*)[_NUM_CYCLE_STATES])states;
    cycle_state_id_t (*tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) transition_table;
    cycle_state_id_t (*cust_tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) cust_tt;

    if(_NUM_CYCLE_STATES != len_states){
        SYS_LOG_WRN("Provided custom state must be of size _NUM_CYCLE_STATES.");
        return;
    }
    if(_NUM_CYCLE_EVENTS != len_events){
        SYS_LOG_WRN("Provided transition table must have dimension 1 ==  _NUM_EVENTS_STATES.");
        return;
    }

    for(int i=0; i<len_states; i++){
        struct State state_cust_cur = cust_states[i];
        // set (usually default) action handler, argument is casted to correct function type 
        state_cust_cur.action = (void (*)(cycle_state_id_t))action;
        // copy into states array
        (*states_p)[i] = cust_states[i];
    }
    for(int i=0; i<len_states; i++){
        for(int j=0; j<len_events; j++){ 
            // set up default (NULL event) transition
            if(j==0)
                (*tt_p)[i][j] = (*states_p)[i].default_next_state;   // states arr must have been filled before
            // just copy
            else
                (*tt_p)[i][j] = (*cust_tt_p)[i][j];   
        } 
    }
}


/*
 * Define actions
 *-----------------------------------------------------------------------------
 */  



void action_print_state(){
    printk("Current state is %i \n", state_mng_get_current());
}


void action_print_start_state(){
    printk("Received start event, state is %i \n", state_mng_get_current());
}

