/**
 *  @brief
 *  @author:
 *  
 * Currently:
 * - Singleton, don't use to set up >1 state machines.
 * - Designed to be only or cooperative thread. Behaviour unclear if not.
 * 
 */

#ifndef TEST_MINIMAL
#include "irqtestperipheral.h"

#define SYS_LOG_DOMAIN "StateManager"  
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>
#include "states.h"



#define STATE_MNG_QUEUE_RX_DEPTH 10

/* modified table-based, event-driven state machine from here
 * http://www.pezzino.ch/state-machine-in-c/
 * - actions are not connected to transitions but the single states
 * - events are gathered over the whole current state
 *   highest prio event determines transitions
 * - if no event: default transition is defined for every state
 */ 

// todo:
// 

enum{
    ABORT_LOOP,
    THREAD_RUNNING,
    _NUM_FLAGS
};

atomic_t flags;


struct Event{
    cycle_event_id_t id_name;
};


/// defines next state id_name on event
static cycle_state_id_t transition_table[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];

/// lists all available states, must match cycle_state_id_t enum order
static struct State _states[_NUM_CYCLE_STATES];
//
/**
 * Internal array storing void to void functions as callbacks.
 * rows: each state
 * columns: different registered callbacks  
 */ 
static void(* _cb_funcs[_NUM_CYCLE_STATES][STATES_CBS_PER_ACTION_MAX])(void);


atomic_t state_cur;                  // save to read from outside, use get_state()
static struct State * _state_cur;   // don't read from outside thread
static struct device * irqt_dev;
K_MSGQ_DEFINE(queue_rx_driver, sizeof(struct DrvEvent), STATE_MNG_QUEUE_RX_DEPTH, 4); // is static
static bool init_done = false;
static u32_t timestamp_reset;


// forward declare
static void _default_action_dispatcher(cycle_state_id_t state);
// forward declare helpers
static struct State * id_2_state(cycle_state_id_t id_name);
void print_state(struct State * state);
void print_arr(irqt_val_id_t arr[], int len);
static bool is_val_in_array(irqt_val_id_t val, irqt_val_id_t *arr, int len);
// forward devlare internal functions
static struct State * switch_state(struct State * current, struct Event * evt);
static struct State * id_2_state(cycle_state_id_t id_name);
static int check_vals_ready(struct State * state);
static int gather_events(struct Event * res_evt);
static void wait(u32_t t_cyc);
static int check_time_goal(struct State * state, int mode, u32_t * t_left);

// PROBLEM: ZEPHYR SW TIMERS ON MS SCALE
// -> so far: no intrinsic hardware timers
// OWN HW TIMERS (COUNTER): 50 cycles to release mutex or queue msg
// -> measure systematic delay and counterbalance?


 // todo: check all functions that write or read _cb_funcs, _states, transition_table



/**
 * @brief Either use default config of states and transition table or set custom one.
 * 
 * If you choose to use custom config, make sure that all your data is properly init!
 * Call first state_mng_configure(), then state_mng_register_action() and state_mng_init().
 * @param: cust_states: Custom 1d state array. Set NULL to use auto.
 * @param: cust_tt:     Custom 2d transition table. Set NULL to use auto.
 * @param: len_state:   Number of elements in cust_states.
 * @paramL len_events:  Number of configured events in cust_tt (second dimension)
 */ 
void state_mng_configure(struct State cust_states[], cycle_state_id_t cust_tt[], int len_states, int len_events){
    if(init_done){
        SYS_LOG_WRN("Can't re-configure after init.");
        return;
    }

    // also: set default action for all states, which just fires all registered actions
    if(cust_states == NULL && cust_tt == NULL)
        states_configure_auto(_states, (cycle_state_id_t *)transition_table, _default_action_dispatcher);
    else if(cust_states != NULL && cust_tt != NULL){
        // just passing arguments to function
        states_configure_custom(_states, (cycle_state_id_t *)transition_table, _default_action_dispatcher, cust_states, cust_tt, len_states, len_events);
    }
    else
        SYS_LOG_WRN("Illegal param combination. Set both or none.");
    /*
    printk("States after config()");
    SYS_LOG_DBG("CYCLE_STATE_IDLE");
    print_state(id_2_state(CYCLE_STATE_IDLE));
    SYS_LOG_DBG("CYCLE_STATE_START");
    print_state(id_2_state(CYCLE_STATE_START));
    SYS_LOG_DBG("CYCLE_STATE_END");
    print_state(id_2_state(CYCLE_STATE_END));
    */
}

/**
 * @brief Initialize. Mainly set up irqt driver. 
 * 
 * Call only after .configure(). Running is possible after init was done.
 */
void state_mng_init(struct device * dev){
    if(init_done){
        SYS_LOG_WRN("Can't re-init after init.");
        return;
    }
    if(dev != NULL){
        irqt_dev = dev;
        // setup irqtester device
        irqtester_fe310_register_queue_rx(dev, &queue_rx_driver);
        // is enabled in run()
        
    }
    else
        SYS_LOG_WRN("Can't set NULL as device");

    init_done = true;

    /*
    printk("States after init()");
    SYS_LOG_DBG("CYCLE_STATE_IDLE");
    print_state(id_2_state(CYCLE_STATE_IDLE));
    SYS_LOG_DBG("CYCLE_STATE_START");
    print_state(id_2_state(CYCLE_STATE_START));
    SYS_LOG_DBG("CYCLE_STATE_END");
    print_state(id_2_state(CYCLE_STATE_END));
    */
}

/**
 * @brief Thread-safe getter whether state machine thread is running.
 */ 
bool state_mng_is_running(){
    return atomic_test_bit(&flags, THREAD_RUNNING);
}

/**
 * @brief Thread-safe getter of current state.
 * @return: id of current state.
 */ 
cycle_state_id_t state_mng_get_current(){
    return atomic_get(&state_cur);
}

/**
 * @brief Send signal to abort running state machine.
 */ 
int state_mng_abort(){
    atomic_set_bit(&flags, ABORT_LOOP);
    SYS_LOG_DBG("Received abort. Expect state manager to stop momentarily...");
    // send dummy event to driver queue to continue if waiting in IDLE
    k_msgq_put(&queue_rx_driver, NULL, K_NO_WAIT);

    int timeout = 100; // ms
    while(state_mng_is_running() && timeout >= 0){
        k_sleep(10);    // switch to state_manager thread
        timeout -= 10;
    }

    if(timeout < 0){
        SYS_LOG_WRN("Didn't shut down in time.");
        return 1;
    }
    return 0;
}


/**
 * @brief Clear callback array and reqvalue arrays for a state.
 * @param: id of state to clear
 * @return: 0 on success, != 0 otherwise.
 */ 
int state_mng_purge_registered_actions(cycle_state_id_t state_id){
    // todo: make threadsafe or prevent while running
    if(init_done){
        SYS_LOG_WRN("Can't modify state machine after init. Call .abort() first.");
        return 1;
    }
    // purge callbacks
    for(int j=0; j<STATES_CBS_PER_ACTION_MAX; j++){
        _cb_funcs[state_id][j] = NULL;
    }
    
    // purge reqvals
    for(int j=0; j<STATES_REQ_VALS_MAX; j++){
        id_2_state(state_id)->val_ids_req[j] = _NIL_VAL;
    }
    
    return 0;
}

/**
 * @brief Clear callback array and reqvalue arrays for all states.
 * @return: 0 on success, != 0 otherwise.
 */ 
int state_mng_purge_registered_actions_all(){
    int res = 0;
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        int res_cur = state_mng_purge_registered_actions(i);
        if(res_cur != 0)    // safe if error return code
            res = res_cur;
    }
    return res;
}


/**
 * @brief Entry function for state machine loop. Should be invoked as thread.
 */ 
void state_mng_run(void){
    
    if(!init_done){
        SYS_LOG_WRN("Can't start state machine before .init().");
        return;
    }
    
    _state_cur = id_2_state(CYCLE_STATE_IDLE);
    // debug to start witouth irq
    // _state_cur = id_2_state(CYCLE_STATE_START);
            
    irqtester_fe310_enable_queue_rx(irqt_dev);
    irqtester_fe310_enable_valflags_rx(irqt_dev);

    struct Event event = {.id_name=_CYCLE_DEFAULT_EVT}; // empty event container
    bool vals_ready = false; 
    u32_t time_left_cyc = 0;

    if(state_mng_is_running()){
        SYS_LOG_WRN("Aborted start. Already running.");
        return;
    }
    atomic_set_bit(&flags, THREAD_RUNNING);
    SYS_LOG_DBG("Start running.");

    while(!atomic_test_bit(&flags, ABORT_LOOP)){
        // check other thread signals?
        
        if(state_mng_get_current() == CYCLE_STATE_IDLE){
            //"waiting" is now done in gather_events()
            //u32_t start_yield = k_cycle_get_32();
            //k_yield(); 
        }
        
        vals_ready = false; 
        time_left_cyc = 0;

        vals_ready = check_vals_ready(_state_cur);
        check_time_goal(_state_cur, 0, &time_left_cyc);
        // todo: handle missed vals or start time

        // fire registered 'action' callbacks
        if(_state_cur->action != NULL)
            _state_cur->action(_state_cur->id_name);
        
        // for clearity, do clear flags here (instead of as action)
        if(_state_cur->id_name == CYCLE_STATE_END)
            irqtester_fe310_clear_all_valflags(irqt_dev);

        time_left_cyc = 0;
        check_time_goal(_state_cur, 1, &time_left_cyc);
        wait(time_left_cyc);

        gather_events(&event);      // highest prio event during this state
        _state_cur = switch_state(_state_cur, &event);
    }

    // clean up
    // todo: state manager clean enough?
    // like this: should be possible to re-init without re-registering actions
    k_msgq_purge(&queue_rx_driver);
    irqtester_fe310_purge_rx(irqt_dev);
    init_done = false; // have to, just purged our driver queue

    SYS_LOG_DBG("Stoped running.");

    atomic_clear_bit(&flags, THREAD_RUNNING);
    atomic_clear_bit(&flags, ABORT_LOOP);
    
}

/**
 * @brief Register an action to be executed in a certain state.
 *         Specify values this action will request from irqt driver.
 * 
 * Currently, it is checked and warned if requested values haven't been flagged
 * ready by the driver.
 * 
 * @param state_id: target state of action
 * @param func:     callback function pointer of action
 * @param arr_vals: array of value ids to request from driver.
 * @param len:      number of elements in arr_vals.
 */
int state_mng_register_action(cycle_state_id_t state_id, void (*func)(void), irqt_val_id_t arr_vals[], int len){
    // todo: make threadsafe
    
    if(state_mng_is_running()){
        SYS_LOG_WRN("Can't register action while running");
        // Used sloppy, import to check whether not running
        return 4;
    }

    if(state_id > _NUM_CYCLE_STATES){
        SYS_LOG_WRN("Couln't register action to unknown state %i", state_id);
        return 3;
    }
    // find first free space in array 
    // array is filled from low index side   
    int i_free = STATES_CBS_PER_ACTION_MAX;
    
    for(int i=0; i<STATES_CBS_PER_ACTION_MAX; i++){
        void (*cb)(void) = _cb_funcs[state_id][i]; // get currrent callback function
        if(cb == NULL){
            i_free = i;     
            break;     
        }  
    }
    
    if(i_free == STATES_CBS_PER_ACTION_MAX){
        SYS_LOG_WRN("Couldn't register action, full array. Increase STATES_CBS_PER_ACTION_MAX?");
        return 1;
    }

    // add new callback
    _cb_funcs[state_id][i_free] = func;
   
    // values will be requested on entering state
    // so add values if not yet present in state.val_ids_req (not ordered)
    // todo: algorithm is horribly inefficient and ugly. Optimize if many values need to be requested.

    // copy not yet exising values in 'to add' array
    irqt_val_id_t arr_vals_to_add[len];
    int j = 0;
    i_free = 0;
    // iterate through arr_vals 
    for(int i=0; i<len; i++){
        irqt_val_id_t val_search = arr_vals[i];
        // check wheter current val_search is already in target array
        if(is_val_in_array(val_search, id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX)){
            ; // nothing to do, value already there
        }
        else{
            arr_vals_to_add[j] = val_search;
            j++;
        }
    }
    int num_vals_to_add = j;

    //SYS_LOG_DBG("Printing vals to add array:");
    //print_arr(arr_vals_to_add, num_vals_to_add);


    //SYS_LOG_DBG("Printing state %i", state_id);
    //print_state(id_2_state(state_id));
    // find free space in target array and save value
    i_free = STATES_REQ_VALS_MAX;
    j = 0;
    for(int i=0; i<STATES_REQ_VALS_MAX; i++){
        if(id_2_state(state_id)->val_ids_req[i] == _NIL_VAL){
            i_free = i; 
            // save new value, if existent
            if(j < num_vals_to_add){
                id_2_state(state_id)->val_ids_req[i_free] = arr_vals_to_add[j];
                j++;
            }
        }
    }


    //SYS_LOG_DBG("Printing for state %i .val_ids_req array:", state_id);
    //print_arr(id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX);

    if(j<num_vals_to_add){
        SYS_LOG_WRN("Couldn't save all requested values for action, full array: Increase STATES_REQ_VALS_MAX?");
        print_arr(id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX);
        return 2;
    }

    return 0;
}


static struct State * switch_state(struct State * current, struct Event * evt){
    cycle_state_id_t next_id;
    cycle_event_id_t evt_id;

    if(evt == NULL)
        evt_id = _CYCLE_DEFAULT_EVT; // = 0: get first element of row
    else
        evt_id = evt->id_name;
    
    if(evt_id >= _NUM_CYCLE_EVENTS || current->id_name >= _NUM_CYCLE_STATES){
        SYS_LOG_ERR("Invalid event id %i or current state id %i", current->id_name, evt_id);
        return NULL;
    }
    next_id = transition_table[current->id_name][evt_id];
    atomic_set(&state_cur, next_id); // to make avalable outside

    return id_2_state(next_id);
} 

static struct State * id_2_state(cycle_state_id_t id_name){
    if(id_name > _NUM_CYCLE_STATES){
        SYS_LOG_ERR("Requested invalid state_id: %i", id_name);
        return NULL;
    }
    return &(_states[id_name]);
}

/**
 * @brief Get priority of a DrvEvent
 * 
 * @param evt:  event to get priority from.
 * @return:     priority, high positive values indicate high priority.
 *              -1: Error. Higher negative values: reserved.
 */
static short get_event_prio(struct DrvEvent * evt){
    switch(evt->irq_id){
        case IRQ_0:
            return 0;
        default:
            SYS_LOG_WRN("Event with unknown irq_id %i", evt->irq_id);
    }
    return -1;
}

/**
 *  @brief Read all events in driver queue and get the one of highest priority.
 * 
 *  If in STATE_IDLE, wait here FOREVER for first drv event.
 *  In case of event with same priority, the one later received (higher index in queue) wins.
 *
 *  @param res_evt: Pointer to state manager event to write result.
 */
static int gather_events(struct Event * res_evt){
    struct DrvEvent drv_evt;

    bool haveRx = false;
    short irq_high_prio = 0;
    short irq_cur_prio = 0;

    // reduces latency of spinning through NULL-event
    int k_wait = K_NO_WAIT;
    if(state_mng_get_current() == CYCLE_STATE_IDLE){
        k_wait = K_FOREVER;
        SYS_LOG_DBG("Going to block in STATE_IDLE, waiting for queue events...");
    }

    // listen to driver queue
    while(0 == k_msgq_get(&queue_rx_driver, &drv_evt, k_wait)){
        // after receiving first queue element (in STATE IDLE), we stop blocking
        k_wait = K_NO_WAIT;
        // filter all wanted IRQs
        if(drv_evt.event_type == EVT_T_IRQ){
            // only return highest prio event
            // currently indicated by highest int value irqt_event_type_t, todo
            irq_cur_prio = get_event_prio(&drv_evt);
            if(irq_cur_prio >= irq_high_prio){
                res_evt->id_name = CYCLE_EVENT_RESET_IRQ;
                irq_high_prio = irq_cur_prio;
            }
        }
        haveRx = true;
        //SYS_LOG_DBG("Received event of type %i from driver", drv_evt.event_type);    
    }
    if(!haveRx){
        res_evt->id_name = _CYCLE_DEFAULT_EVT;
        return 1;
    }
    SYS_LOG_DBG("Chosen relevant state event id %i", res_evt->id_name);    
    
    return 0;

}


static int check_vals_ready(struct State * state){
    
    bool val_i_ready = false;
    bool all_ready = true;
    
    for(int i=0; i<STATES_REQ_VALS_MAX; i++){
        // empty array entry means no more vals to check
        if(state->val_ids_req[i] == _NIL_VAL)
            break;
        val_i_ready = irqtester_fe310_test_valflag(irqt_dev, state->val_ids_req[i]);

        if(!val_i_ready){
            SYS_LOG_WRN("Flag for requesting value %i from driver unready", state->val_ids_req[i]);
            all_ready = false;
        }
    }

    return all_ready;
}

/** 
 *   @brief Calcs the time since start state.
 *   
 *   Warning: requires that delta to start state can never > 2^32 cycles. 
 *   @return: delta in cycles
 */ 
static u32_t get_time_delta(struct State * state){
 
    return (k_cycle_get_32() - timestamp_reset);    
}

/**
 * @brief Wait for at least t_cyc cycles.
 * 
 * No upper limit of waiting time guaranteed!
 * Busy waiting currently, since no hw timers implemented yet.
 */ 

static void wait(u32_t t_cyc){
    u32_t start = k_cycle_get_32();
    u32_t now = start;
    
    while(t_cyc < (now - start)){
        now = k_cycle_get_32();
    }

}

/** 
 * @param t_left:   if goal met: cycles still left to goal, else 0   
 * @param mode:     0: check start goad, 1:end goal
 * @return:         0: goal met
 */
static int check_time_goal(struct State * state, int mode, u32_t * t_left){

    *t_left = 0;

    if(state->id_name == CYCLE_STATE_IDLE){
        return 0;
    }

    if(state->id_name == CYCLE_STATE_START && (mode == 0)){
         // implicitly sets timer at beginning of start state (before .action)
        timestamp_reset = k_cycle_get_32();  
    }

    u32_t delta_cyc;
    u32_t goal;
    goal = (mode == 0) ? (state->timing_goal_start) : (state->timing_goal_end);
    delta_cyc = get_time_delta(state);


    // attention: delta and goal are both time differences since timestamp_reset 
    if(delta_cyc > goal){
        SYS_LOG_WRN("Timing goal %i = %i cyc missed for state %i, now: %i", \
                    mode, goal, state->id_name, delta_cyc);
        return 1;
    }

    *t_left = goal - delta_cyc;

    return 0;
}


// helper
static bool is_val_in_array(irqt_val_id_t val, irqt_val_id_t *arr, int len){
    int i;
    for (i=0; i < len; i++) {
        if (arr[i] == val)
            return true;
    }
    return false;
}

// helper
void print_arr(irqt_val_id_t arr[], int len){
    int i;
    for (i=0; i < len; i++) {
        printk("%i, ", (int)arr[i]);
    }
    printk("\n");
}

void print_state(struct State * state){
    printk("Printing state struct: \n");
    printk("id: %i \ndefault_next: %i \ngoal_start: %u \ngoal_end: %u \nval_ids_req \n", \
            state->id_name, state->default_next_state,     \
            state->timing_goal_start, state->timing_goal_end);
    print_arr(state->val_ids_req, STATES_REQ_VALS_MAX);
    printk("action: %p \n", state->action);

}

/**
 *  @brief Fires all callbacks stored in internal array.
 * 
 *  Use state_manager::register_action() and ::purge_registered_action() to access.
 */
static void _default_action_dispatcher(cycle_state_id_t state){
    for(int i=0; i<STATES_CBS_PER_ACTION_MAX; i++){
        void (*cb)(void) = _cb_funcs[state][i];
        if(cb != NULL){
            // fire callback!
            cb();
        }
    }
}

#endif // TEST_MINIMAL