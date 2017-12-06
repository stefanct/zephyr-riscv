#include "states.h"
#include "state_manager.h"

// forward declare actions
static void action_print_state();
static void action_print_start_state();


/*
* Define states
*-----------------------------------------------------------------------------
*/

static struct State s_idle = {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0,
        .val_ids_req = {}, .action = NULL 
        };

static struct State s_start = {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_END,
            .timing_goal_start = 0, .timing_goal_end = 0,
            .val_ids_req = {}, .action = action_print_start_state 
            };

static struct State s_end = {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0,
        .val_ids_req = {}, .action = action_print_state 
        };

void states_configure(struct State * states, cycle_state_id_t * transition_table){
   
    // cast to pointers to array with correct dimensions
    struct State (*states_p)[_NUM_CYCLE_STATES]  = (struct State (*)[_NUM_CYCLE_STATES])states;
    cycle_state_id_t (*tt_p)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS] = (cycle_state_id_t (*)[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS]) transition_table;

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

/*
 * Define actions
 *-----------------------------------------------------------------------------
 */  

static void action_print_state(){
    printk("Current state is %i \n", state_mng_get_current());
}


static void action_print_start_state(){
    printk("Received start event, state is %i \n", state_mng_get_current());
}




