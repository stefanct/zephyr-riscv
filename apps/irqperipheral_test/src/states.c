#ifndef TEST_MINIMAL
#include "states.h"
#include "state_manager.h"
#include "utils.h"

#define SYS_LOG_DOMAIN "States"  
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>
#include <string.h>

/**
 * Defines a minimal set of states
 * Use state_manager::register_action() to set state actions and required values.
 * CHANGES may make benchmarks based on SM auto uncomparable!
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
 * Uses auto SM states hard-coded in states.c.
 * Is used for tests/benchs with a minimal critical loop.
 * Registers a (default) action handler to every state, which is a function
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
    (*states_p)[CYCLE_STATE_IDLE] = s_idle;
    (*states_p)[CYCLE_STATE_START] = s_start;
    (*states_p)[CYCLE_STATE_END] = s_end;

    // manually set up transition table[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]
    // for CYCLE_STATE_IDLE
    cycle_event_id_t t0[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[CYCLE_STATE_IDLE].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[CYCLE_STATE_IDLE][_CYCLE_DEFAULT_EVT] = t0[0];   
    (*tt_p)[CYCLE_STATE_IDLE][CYCLE_EVENT_RESET_IRQ] = t0[1];  
    // for CYCLE_STATE_START    
    cycle_event_id_t t1[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[CYCLE_STATE_START].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[CYCLE_STATE_START][_CYCLE_DEFAULT_EVT] = t1[0];   
    (*tt_p)[CYCLE_STATE_START][CYCLE_EVENT_RESET_IRQ] = t1[1];   
    // for CYCLE_STATE_END        
    cycle_event_id_t t2[_NUM_CYCLE_EVENTS] = \
        {(*states_p)[CYCLE_STATE_END].default_next_state, CYCLE_STATE_START};  
    (*tt_p)[CYCLE_STATE_END][_CYCLE_DEFAULT_EVT] = t2[0];   
    (*tt_p)[CYCLE_STATE_END][CYCLE_EVENT_RESET_IRQ] = t2[1];   

}

/**
 * @brief: Set up state array and transition table of state manager by customnly deployed arrays. 
 * 
 * Registers a default action handler to every state, which is a function
 * that is responsible to fire all registered callbacks. 
 * Non thread safe, never call if sm running
 * 
 * @param states:           Pointer to state_manager state array.
 * @param transition_table: Pointer to state_manager transition_table.
 * @param action:           Function pointer (void-2-void) to default action, usually state manager action dispatcher.
 * @param cust_states[]:    All states to be configured to state machine. Must be static var. todo: really? Copying into sm?
 * @param cust_tt[]:        Transition table to be configure to state machine. Must be static var.
 */
void states_configure_custom(struct State * states, cycle_state_id_t * transition_table, void * action, \
                                struct State cust_states[], cycle_state_id_t * cust_tt, int len_states, int len_events){
    
    // cast to pointers to array with correct dimensions
    struct State (*states_p)[_NUM_CYCLE_STATES]  = (struct State (*)[_NUM_CYCLE_STATES])states;
    cycle_state_id_t (*tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) transition_table;
    cycle_state_id_t (*cust_tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) cust_tt;

    if(_NUM_CYCLE_STATES != len_states){
        SYS_LOG_WRN("Provided custom state must be of size _NUM_CYCLE_STATES.");
        return;
    }
    if(_NUM_CYCLE_EVENTS != len_events){
        SYS_LOG_WRN("Provided transition table must have dimension 1 == _NUM_EVENTS_STATES.");
        return;
    }

    // set (usually default) action handler, argument is casted to correct function type 
    for(int i=0; i<len_states; i++)
        cust_states[i].action = (void (*)(cycle_state_id_t))action;

    // copy into state_manger states array
    memcpy(*states_p, cust_states, len_states * sizeof(*states)) ;

    for(int i=0; i<len_states; i++){
        for(int j=0; j<len_events; j++){
            // copy into state_manager transition table
            (*tt_p)[i][j] = (*cust_tt_p)[i][j];
        }
        // set up default (NULL event) transition
        (*tt_p)[i][0] = (*states_p)[i].default_next_state;   // states arr must have been filled before
    }
}

/**
 * @brief: Configure given state with serial substates.
 * todo: make safe while sm running
 * 
 * @param num_substates: number of times the state is repeated
 * @param timing_summand: usually 0
 */
void states_configure_substates(struct State * state, u8_t num_substates, u8_t timing_summand){
    if(num_substates < 1){
        SYS_LOG_WRN("Invalid number of substates %i, set to 1", num_substates);
        num_substates = 1;
    }
    state->max_subs_idx = num_substates - 1;    // idx 0 counts as substate
    state->timing_summand = timing_summand;
}


void states_print_state(struct State * state){
    printk("Printing state struct: \n");
    printk("id: %i \ndefault_next: %i \ngoal_start: %u \ngoal_end: %u \nval_ids_req \n", \
            state->id_name, state->default_next_state,     \
            state->timing_goal_start, state->timing_goal_end);
    print_arr_uint(0, state->val_ids_req, STATES_REQ_VALS_MAX);
    printk("action: %p \n", state->action);

}


int states_get_timing_goal_start(struct State * state, u8_t substate){
    
   
    int result = state->timing_goal_start;   

    // substate logic
    // replicate duration of parent state + summand
    #if(STATES_DIS_SUBSTATES == 0)
    if(likely(state->max_subs_idx > 0))
        result += substate * (state->timing_goal_end - state->timing_goal_start + state->timing_summand);
    #endif

    return result;
}

int states_get_timing_goal_end(struct State * state, u8_t substate){
    
    int result = state->timing_goal_end;

    // substate logic
    // replicate duration of parent state + summand
    #if(STATES_DIS_SUBSTATES == 0)
    if(likely(state->max_subs_idx > 0))
        result += substate * (state->timing_goal_end - state->timing_goal_start + state->timing_summand) ;
    #endif

    return result;
}

int states_set_handler_timing_goal_start(struct State state_arr[], cycle_state_id_t id, void(*handler)(struct State *, int)){
    struct State * state = states_get(state_arr, id);
    if(state->handle_t_goal_start != NULL)
        SYS_LOG_WRN("Overwriting timing goal start handler for state %i", id);
    state->handle_t_goal_start = handler;

    return 0;
}

int states_set_handler_timing_goal_end(struct State state_arr[], cycle_state_id_t id, void(*handler)(struct State *, int)){
    struct State * state = states_get(state_arr, id);
    if(state->handle_t_goal_end != NULL)
        SYS_LOG_WRN("Overwriting timing goal end handler for state %i", id);
    state->handle_t_goal_end = handler;

    return 0;
}

int states_set_handler_reqval(struct State state_arr[], cycle_state_id_t id, bool(*handler)(struct State *)){
    struct State * state = states_get(state_arr, id);
    if(state->handle_val_rfail != NULL)
        SYS_LOG_WRN("Overwriting failed requested value handler for state %i", id);
    state->handle_val_rfail = handler;

    return 0;
}

struct State * states_get(struct State state_arr[], cycle_state_id_t id){
    return &(state_arr[id]);
}

#endif

