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
#include "state_manager.h"
#include "irqtestperipheral.h"

#define SYS_LOG_DOMAIN "StateManager"  
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>
#include "log_perf.h"
#include "states.h"
#include "utils.h"
#include "cycles.h"
#include "globals.h"

#define STATE_MNG_QUEUE_RX_DEPTH 5
// log using Switch_Event and Wait_Event
// fast way to log
#define STATE_MNG_LOG_EVENTS_DEPTH CONFIG_APP_SM_LOG_DEPTH  // 0 to deactivate log 
// can log using LOG_PERF
// LOG_PERF is faster than SYS_LOG
// but still slow! (~ 5000 cyc)
#define STATE_MNG_LOG_PERF  

#define LEN_ARRAY(x) \
	((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
	// call only on arrays defined in scope


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
    THREAD_START,
    _NUM_FLAGS
};

static atomic_t flags;

/// Controls switching of state_manager states
struct SMEvent{
    cycle_event_id_t id_name;
};

/// for logging
static struct Switch_Event log_switch_evts[STATE_MNG_LOG_EVENTS_DEPTH];
static struct Perf_Event log_perf_evts[STATE_MNG_LOG_EVENTS_DEPTH];
static struct Wait_Event log_wait_evts[STATE_MNG_LOG_EVENTS_DEPTH];
static cycle_state_id_t log_filter[_NUM_CYCLE_STATES] = {_NIL_CYCLE_STATE};   // log_filter[0] = _NIL_CYCLE_STATE => no filter


/// defines next state id_name on event
static cycle_state_id_t transition_table[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];

/// lists all available states, must match cycle_state_id_t enum order
static struct State _states[_NUM_CYCLE_STATES];
//
/**
 * Internal array storing callbacks. Get passed a read-only (const) ActionArg on call.
 * rows: each state
 * columns: different registered callbacks  
 */ 
static void(* _cb_funcs[_NUM_CYCLE_STATES][STATES_CBS_PER_ACTION_MAX])(struct ActionArg const *);


static atomic_t state_cur_id;               // save to read from outside, use get_state()
static struct State * _state_cur;   // don't read from outside thread
static struct device * irqt_dev;
K_MSGQ_DEFINE(queue_rx_driver, sizeof(struct DrvEvent), STATE_MNG_QUEUE_RX_DEPTH, 4); // is static
static bool init_done = false;
static u32_t timestamp_reset;


// forward declare
static void _default_action_dispatcher(cycle_state_id_t state);

static bool is_val_in_array(irqt_val_id_t val, irqt_val_id_t *arr, int len);
static bool is_state_in_array(cycle_state_id_t state, cycle_state_id_t *arr, int len);
static void mes_perf(struct State * state, int mode);
// forward devlare internal functions
static struct State * switch_state(struct State * current, struct SMEvent * evt);
static int gather_events(struct SMEvent * res_evt);
static void check_time_goal_start(struct State * state, int * t_left);
static void check_time_goal_end(struct State * state, int * t_left);
static void handle_time_goal_start(struct State * state, int t_left);
static void handle_time_goal_end(struct State * state, int t_left);
static bool handle_vals_ready(struct State * state);
static int state_mng_check_vals_ready(struct State * state);
static void state_mng_purge_evt_log();
    



/**
 * @brief Either use default config of states and transition table or set custom one.
 * 
 * If you choose to use custom config, make sure that all your data is properly init!
 * Call first state_mng_configure(), then state_mng_register_action() and state_mng_init().
 * @param cust_states: Custom 1d state array. Set NULL to use auto.
 * @param cust_tt:     Custom 2d transition table. Set NULL to use auto.
 * @param len_state:   Number of elements in cust_states.
 * @param len_events:  Number of configured events in cust_tt (second dimension)
 */ 
void state_mng_configure(struct State cust_states[], cycle_state_id_t * cust_tt, int len_states, int len_events){
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
    
    // warn if substates configured but state manager does not support
    #if(STATES_DIS_SUBSTATES == 1)
    for(int i=0; i < _NUM_CYCLE_STATES; i++){
        if(cust_states[i].max_subs_idx != 0)
            SYS_LOG_WRN("State manager does not support substates, but configured %i in state %i.", cust_states[i].max_subs_idx+1, cust_states[i].id_name);
    }
    #endif

}


/**
 * @brief Print info for all states configured to state_manager
 */
void state_mng_print_state_config(){

    SYS_LOG_INF("State config:");

    for(int i=_NIL_CYCLE_STATE + 1; i<_NUM_CYCLE_STATES; i++){
        struct State * state_cur = &(_states[i]);
        int j = 0;
        do{
            short next_substate = (j+1 > state_cur->max_subs_idx ? 0 : j+1);
            int next_state = (next_substate == 0 ? state_cur->default_next_state : state_cur->id_name);
            // State array is created with size of cycle_state_id_t
            // but no need for every sm to configure all states
            // unconfigured states are never reached
            if(i != state_cur->id_name){
                SYS_LOG_INF("State %i <disabled>", i);
            }
            else{
                char str_buf_1[30];
                char str_buf_2[100];
                snprint_arr_int(str_buf_1, 20, (int*)state_cur->val_ids_req, STATES_REQ_VALS_MAX);
                snprint_arr_p(str_buf_2, 40, (void (**))(_cb_funcs[i]), STATES_CBS_PER_ACTION_MAX);
               
                SYS_LOG_INF("State %i.%u -> %i.%u:\n\t t_goal_start/end= [%u - %u] cyc \n" \
                        "\t handler_start/end: %p, %p \n" \
                        "\t vals_requested: %s \n" \
                        "\t actions: %s",

                state_cur->id_name, j, 
                next_state, next_substate,
                state_mng_get_timing_goal(state_cur, j, 0), 
                state_mng_get_timing_goal(state_cur, j, 1),
                state_cur->handle_t_goal_start, 
                state_cur->handle_t_goal_end,
                str_buf_1, str_buf_2);
                
            }
            
            j++;
        }while(j <= state_cur->max_subs_idx);
    }
}

/**
 * @brief Print transition table as configured to state_manager
 */
void state_mng_print_transition_table_config(){

    SYS_LOG_INF("Transition table config:");
    for(int i=_NIL_CYCLE_STATE + 1; i<_NUM_CYCLE_STATES; i++){
        struct State * state_cur = &(_states[i]);

        SYS_LOG_INF("State %i%s transitions:", i, (i != state_cur->id_name ? " <disabled>" : "")); 

        char string[80];
        cycle_state_id_t * tt_line = transition_table[i];
        snprint_arr_int(string, 80, (int *) tt_line, _NUM_CYCLE_EVENTS);
        
        SYS_LOG_INF("-> %s", string);
    }
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

    // in case state_mng ran before this new init
    state_mng_purge_evt_log();
    init_done = true;

    /*
    printk("States after init()");
    SYS_LOG_DBG("CYCLE_STATE_IDLE");
    states_print_state(state_mng_id_2_state(CYCLE_STATE_IDLE));
    SYS_LOG_DBG("CYCLE_STATE_START");
    states_print_state(state_mng_id_2_state(CYCLE_STATE_START));
    SYS_LOG_DBG("CYCLE_STATE_END");
    states_print_state(state_mng_id_2_state(CYCLE_STATE_END));
    */
}

/**
 * @brief Thread-safe getter whether state machine thread is running.
 */ 
bool state_mng_is_running(){
    return atomic_test_bit(&flags, THREAD_RUNNING);
}

/**
 * @brief Thread-safe getter of current state id.
 * @return id of current state.
 */ 
cycle_state_id_t state_mng_get_current(){
    return atomic_get(&state_cur_id);
}
/**
 * @brief Thread-unsafe getter of current substate index.
 *        Call ONLY from within state_mng run loop.
 * @return id of current state.
 */ 
u8_t state_mng_get_current_subs(){
    struct State * state_cur = state_mng_id_2_state(state_mng_get_current()); 
    return state_cur->cur_subs_idx;
    
}
/**
 * @brief Thread-unsafe getter of switch events
 *        Call ONLY if SM is certain not to switch state.
 * 
 * @return 0 on success. 1 if buf is too small, return data truncated
 */
int state_mng_get_switch_events(struct Switch_Event * buf, int len_buf){

    int j_found = 0;
    int j_buf = 0;
    for(int i=0; i<LEN_ARRAY(log_switch_evts); i++){
        if(log_switch_evts[i].from_state != _NIL_CYCLE_STATE){  // filter out empty events
            if(j_found < len_buf)
                buf[j_buf++] = log_switch_evts[i];
            j_found++;
        }
    }

    // write un-init value
    struct Switch_Event uninit = {.from_state = _NIL_CYCLE_STATE, .to_state = _NIL_CYCLE_STATE};
    for(int i = j_buf; i < len_buf; i++)
        buf[i] = uninit;

    if(j_found <= len_buf)
        return 0;

    return 1;
}

int state_purge_switch_events(){
    struct Switch_Event empty = {.from_state = _NIL_CYCLE_STATE,
         .to_state = _NIL_CYCLE_STATE, .t_delta_cyc=0, .t_cyc=0};
    for(int i=0; i<LEN_ARRAY(log_switch_evts); i++){
        log_switch_evts[i] = empty;
    }
    return 0;
}


/**
 * Add a state_id to the logging filter.
 * Only added states are logged by events.
 * 
 * @return 0 on sucess, 1 if filter array full
 */
int state_mng_add_filter_state(cycle_state_id_t state_id){
    for(int i=0; i<LEN_ARRAY(log_filter); i++){
        if(log_filter[i] == _NIL_CYCLE_STATE){
            log_filter[i] = state_id;
            return 0;
        }
    }
    return 1;
}

int state_mng_purge_filter_state(){
    for(int i=0; i<LEN_ARRAY(log_filter); i++){
        log_filter[i] = _NIL_CYCLE_STATE;
    }
    
    return 0;
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
 * @param state_id: id of state to clear
 * @return 0 on success, != 0 otherwise.
 */ 
int state_mng_purge_registered_actions(cycle_state_id_t state_id){

    if(init_done || state_mng_is_running()){
        SYS_LOG_WRN("Can't modify state machine after init. Call .abort() first.");
        return 1;
    }
    // purge callbacks
    for(int j=0; j<STATES_CBS_PER_ACTION_MAX; j++){
        _cb_funcs[state_id][j] = NULL;
    }
    
    // purge reqvals
    for(int j=0; j<STATES_REQ_VALS_MAX; j++){
        state_mng_id_2_state(state_id)->val_ids_req[j] = _NIL_VAL;
    }
    
    return 0;
}

/**
 * @brief Clear callback array and reqvalue arrays for all states.
 * @return 0 on success, != 0 otherwise.
 */ 
int state_mng_purge_registered_actions_all(){
    int res = 0;
    for(int i=_NIL_CYCLE_STATE + 1; i<_NUM_CYCLE_STATES; i++){
        int res_cur = state_mng_purge_registered_actions(i);
        if(res_cur != 0)    // safe if error return code
            res = res_cur;
    }
    return res;
}

/**
 * @brief Send signal to start running state machine.
 */ 
int state_mng_start(){
    atomic_set_bit(&flags, THREAD_START);

    SYS_LOG_DBG("Received start to run sm loop.");

    int timeout = 100; // ms
    while(!state_mng_is_running() && timeout >= 0){
        k_sleep(10);    // switch to state_manager thread
        timeout -= 10;
    }

    if(timeout < 0){
        SYS_LOG_WRN("Didn't start in time. Did you create a thread?");
        return 1;
    }

    return 0;
}


/**
 * @brief Entry function for state machine (FSM) loop. 
 *        Should be invoked as thread; do NOT call.
 * 
 * Notes:
 * - FE310: Freedom/With1TinyCore sets btb = None -> no dynamic branch prediction 
 * - For static optimization, from ISA: Assume that backward branches 
 *   will be predicted taken, forward branches as not taken.
 * - Attention: depending in gcc version likely() macros may not work as expected
 *   check assembly! (not properly working on riscv-gcc: 6.1.0 / 7.1.1)
 * - Implementation detail: Waiting on start time goals and vals ready could be 
 *   related to transitions in the FSM. Here, included in state to keep transition 
 *   table small and allow for more flexibility (-> handler for missed time goals)
 */ 
void state_mng_run(void){
    // block here before start()
    while(!atomic_test_bit(&flags, THREAD_START)){
        k_sleep(10);
        // skip start if .abort() is called before .start()
        // no clean up called, so driver queue still registered afterwards
        if(atomic_test_bit(&flags, ABORT_LOOP)){
             atomic_clear_bit(&flags, ABORT_LOOP);
             return;
        }
    }

    if(!init_done){
        SYS_LOG_WRN("Can't start state machine before .init().");
        return;
    }

    if(state_mng_is_running()){
        SYS_LOG_WRN("Aborted start. Already running.");
        return;
    }
    
    _state_cur = state_mng_id_2_state(CYCLE_STATE_IDLE);
    atomic_set(&state_cur_id, CYCLE_STATE_IDLE);
    // debug to start witouth irq
    // _state_cur = state_mng_id_2_state(CYCLE_STATE_START);

    // enable driver to send events up   
    if(0 != irqtester_fe310_enable_queue_rx(irqt_dev))
        return;
    irqtester_fe310_enable_valflags_rx(irqt_dev);

    atomic_set_bit(&flags, THREAD_RUNNING);
    SYS_LOG_DBG("SM thread entering loop, thread %p.", k_current_get());

    while(!atomic_test_bit(&flags, ABORT_LOOP)){
        // check other thread signals?
        bool vals_ready = false; 
        bool skip_actions = false;
        int time_left_cyc = 0;
        struct SMEvent event = {.id_name=_CYCLE_DEFAULT_EVT}; // empty event container

        check_time_goal_start(_state_cur, &time_left_cyc);
        handle_time_goal_start(_state_cur, time_left_cyc);
     
        // check if requested values of state have set valflag
        vals_ready = state_mng_check_vals_ready(_state_cur);

        if(unlikely(!vals_ready)){
            // call state handler, may wait until valflag set
            skip_actions = handle_vals_ready(_state_cur);
            // clear valflags if necessary in action
        }
        // measure performance of actions, after waiting done
        mes_perf(_state_cur, 0);
        // fire registered 'action' callbacks
        if(likely(!skip_actions && _state_cur->action != NULL)) 
            _state_cur->action(_state_cur->id_name);
        
        // for clearity, do clear flags here (instead of as action)
        if(unlikely(_state_cur->id_name == CYCLE_STATE_END))
            irqtester_fe310_clear_all_valflags(irqt_dev);

        mes_perf(_state_cur, 1);

        time_left_cyc = 0;
        check_time_goal_end(_state_cur, &time_left_cyc);
        handle_time_goal_end(_state_cur, time_left_cyc);

        gather_events(&event);      // get highest prio event during this state
        _state_cur = switch_state(_state_cur, &event);
    }

    // clean up
    // possible to re-init without re-registering actions
    k_msgq_purge(&queue_rx_driver);
    irqtester_fe310_purge_rx(irqt_dev);
    init_done = false; // have to, just purged our driver queue

    SYS_LOG_DBG("Stopped running.");

    atomic_clear_bit(&flags, THREAD_RUNNING);
    atomic_clear_bit(&flags, THREAD_START);
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
int state_mng_register_action(cycle_state_id_t state_id, void (*func)(struct ActionArg const *), irqt_val_id_t arr_vals[], int len){
    
    if(state_mng_is_running()){
        SYS_LOG_WRN("Can't register action while running");
        return 4;
    }

    if(state_id > _NUM_CYCLE_STATES || state_id < CYCLE_STATE_IDLE){
        SYS_LOG_WRN("Couln't register action to unknown state %i", state_id);
        return 3;
    }
    // find first free space in array 
    // array is filled from low index side ("bottom up")
    int i_free = STATES_CBS_PER_ACTION_MAX;
    
    for(int i=0; i<STATES_CBS_PER_ACTION_MAX; i++){
        void (*cb)(struct ActionArg const *) = _cb_funcs[state_id][i]; // get currrent callback function
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
        if(is_val_in_array(val_search, state_mng_id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX)){
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
    //states_print_state(state_mng_id_2_state(state_id));
    // find free space in target array and save value
    i_free = STATES_REQ_VALS_MAX;
    j = 0;
    for(int i=0; i<STATES_REQ_VALS_MAX; i++){
        if(state_mng_id_2_state(state_id)->val_ids_req[i] == _NIL_VAL){
            i_free = i; 
            // save new value, if existent
            if(j < num_vals_to_add){
                state_mng_id_2_state(state_id)->val_ids_req[i_free] = arr_vals_to_add[j];
                j++;
            }
        }
    }

    if(j<num_vals_to_add){
        SYS_LOG_WRN("Couldn't save all requested values for action, full array: Increase STATES_REQ_VALS_MAX?");
        //print_arr_uint(state_mng_id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX);
        return 2;
    }
    /*
    SYS_LOG_DBG("Successfully registered action %p for state %i", func, state_id);
    SYS_LOG_DBG("Now actions:");
    for(int i=0; i<STATES_CBS_PER_ACTION_MAX; i++){
        printk("%p, ", _cb_funcs[state_id][i]);
    }
    printk("\n");
    */
    //SYS_LOG_DBG("Now requested values:");
    //print_arr_uint(state_mng_id_2_state(state_id)->val_ids_req, STATES_REQ_VALS_MAX);


    return 0;
}

/**
 * @brief Print logged switch, wait and performance mes. events
 */ 
void state_mng_print_evt_log(){
    SYS_LOG_DBG("Last %i switch events:", STATE_MNG_LOG_EVENTS_DEPTH);
    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        #if STATE_MNG_LOG_EVENTS_DEPTH != 0 // just for compiler zero-div warning, unnecessary
        cycle_state_id_t to_id = log_switch_evts[i].to_state;
        if(to_id == _NIL_CYCLE_STATE)
            continue;
        u8_t to_sub_id = log_switch_evts[i].to_substate;
        cycle_state_id_t from_id = log_switch_evts[i].from_state;
        u8_t from_sub_id = log_switch_evts[i].from_substate;
        u32_t t_delta = log_switch_evts[i].t_delta_cyc;
        u32_t t_cyc = log_switch_evts[i].t_cyc;
        u32_t t_last = (i>0 ? log_switch_evts[(i-1)%STATE_MNG_LOG_EVENTS_DEPTH].t_cyc : 0);
        
        SYS_LOG_DBG("[%u / %u] Switching %i.%u -> %i.%u after %u cyc", t_delta, t_cyc,
            from_id, from_sub_id, to_id, to_sub_id, t_cyc - t_last);
       #endif
    }

    SYS_LOG_DBG("Last %i wait events:", STATE_MNG_LOG_EVENTS_DEPTH);
    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        cycle_state_id_t id = log_wait_evts[i].state;
        if(id == _NIL_CYCLE_STATE)
            continue;
        u32_t t_wait = log_wait_evts[i].t_wait_cyc;
        u8_t sub_id = log_wait_evts[i].substate;
        sm_wait_reason_t reason = log_wait_evts[i].reason;
        SYS_LOG_DBG("Waited %u cyc, reason %i in %i.%u", t_wait, reason, id, sub_id);
    }

    SYS_LOG_DBG("Last %i perf mes events:", STATE_MNG_LOG_EVENTS_DEPTH);
    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        cycle_state_id_t id = log_perf_evts[i].state;
        if(id == _NIL_CYCLE_STATE)
            continue;
        u8_t sub_id = log_perf_evts[i].substate;
        u32_t t_action = log_perf_evts[i].t_actions_cyc;
        SYS_LOG_DBG("Actions for state %i.%u took %u cyc", id, sub_id, t_action);
    }

}

/**
 * @brief   Debug function, likely to be removed.
 *          Get and delete all log events for certain id_state
 * @return  idx of buffer last written
 * 
 */

int state_mng_pull_perf_action(int res_buf[], int idx_buf, int len){

    // fill in with fitting values form event log
    int i=idx_buf;
    int overflow = 0;
    for(int j_log=0; j_log<STATE_MNG_LOG_EVENTS_DEPTH; j_log++){
        if(log_perf_evts[j_log].state == CYCLE_STATE_IDLE)
            continue;
            // by definition no action in IDLE state
        if(i<len){
            res_buf[i] = log_perf_evts[j_log].t_actions_cyc;
            // "delete" log element
            log_perf_evts[j_log].state = CYCLE_STATE_IDLE;
            log_perf_evts[j_log].t_actions_cyc = 0;
            i++;
        }
        if(i >= len){
            i=0;  // full buffer, start writing at beginning again
            overflow++;
        }
    }

    // warn if buffer to small to save all evts
    if((overflow == 1 && i >= idx_buf) || overflow > 1)
        SYS_LOG_WRN("Supplied buffer too small. Some values overwritten");
    
    return i;
}


static struct State * switch_state(struct State * current, struct SMEvent * evt){
    cycle_state_id_t next_id = 0;
    cycle_event_id_t evt_id = 0;

    if(unlikely(evt == NULL))
        evt_id = _CYCLE_DEFAULT_EVT; // = 0: get first element of row
    else
        evt_id = evt->id_name;
    
    if(unlikely(evt_id >= _NUM_CYCLE_EVENTS || current->id_name >= _NUM_CYCLE_STATES)){
        SYS_LOG_ERR("Invalid event id %i or current state id %i", current->id_name, evt_id);
        return _NIL_CYCLE_STATE;
    }

    // substate logic
    // repeat this state if not all substates already occured and default evt
    u8_t * cur_subs_p = &(current->cur_subs_idx); // value is 0, if max_subs_idx == 0
    u8_t next_subs = 0;

    bool done_substates = true;
#if(STATES_DIS_SUBSTATES == 0)
    
    if(likely(current->max_subs_idx > 0)){  // state has substates
        done_substates = false;
        next_subs = current->cur_subs_idx + 1;
        
        // all substates occured 
        // or non-default event forces switching
        if(unlikely(next_subs > current->max_subs_idx || evt_id != _CYCLE_DEFAULT_EVT)){
            //reset 
            next_subs = 0;
            done_substates = true; 
        }
        else{
            // repeat current parent state if no special event
            next_id = current->id_name;
        }
    }
#endif
    if(unlikely(done_substates)){
        next_id = transition_table[current->id_name][evt_id];
        atomic_set(&state_cur_id, next_id); // to make available outside
    }

    // log and debug 
    #if(STATE_MNG_LOG_EVENTS_DEPTH > 0)
     // skip if state is not listed in filter
        if((is_state_in_array(current->id_name, log_filter, LEN_ARRAY(log_filter))) \
            || (is_state_in_array(next_id, log_filter, LEN_ARRAY(log_filter))) \
            || log_filter[0] == _NIL_CYCLE_STATE ){

            static u32_t i_switch;  // attention: log corruption if overflows
            u32_t t_delta = state_mng_get_time_delta();
            u32_t t_cyc = get_cycle_32();
            /* dbg
            u32_t reg = i_switch;
            __asm__ volatile("csrw mtvec, %0" :: "r" (reg)); 
            void __irq_wrapper();
            reg = __irq_wrapper;
            __asm__ volatile("csrw mtvec, %0" :: "r" (reg)); 
            */
            struct Switch_Event * s_evt_p = &(log_switch_evts[i_switch % STATE_MNG_LOG_EVENTS_DEPTH]);
            s_evt_p->t_delta_cyc = t_delta;
            s_evt_p->t_cyc = t_cyc;
            s_evt_p->from_state = current->id_name;
            s_evt_p->from_substate = current->cur_subs_idx;
            s_evt_p->to_state = next_id;
            s_evt_p->to_substate = next_subs;
             
            //SYS_LOG_DBG("[%u / %u] Switching %i.%u -> %i.%u", s_evt_p->t_delta_cyc, s_evt_p->t_cyc,
            //   s_evt_p->from_state, s_evt_p->from_substate, s_evt_p->to_state, s_evt_p->to_substate);
              
        #ifdef STATE_MNG_LOG_PERF
            // log only if t_last is available
            if(i_switch > (i_switch-1) % STATE_MNG_LOG_EVENTS_DEPTH){
                u32_t t_last = log_switch_evts[(i_switch-1) % STATE_MNG_LOG_EVENTS_DEPTH].t_cyc;
                LOG_PERF("[%u / %u] Switching %i.%u -> %i.%u after %u cyc", s_evt_p->t_delta_cyc, s_evt_p->t_cyc,
                    s_evt_p->from_state, s_evt_p->from_substate, s_evt_p->to_state, s_evt_p->to_substate, t_cyc - t_last);
            }
        #endif

            i_switch++;
        }
    #endif
    
    // set timestamp if switching so START
    if(unlikely(next_id == CYCLE_STATE_START)){
        timestamp_reset = get_cycle_32();  
    }

    #if(STATES_DIS_SUBSTATES == 0)
    *cur_subs_p = next_subs;
    #endif

    // debug
    //if(state_mng_get_current() == CYCLE_STATE_IDLE)
    //SYS_LOG_DBG("current id %i, next id %i, state %p", current->id_name, next_id, state_mng_id_2_state(next_id));
 
    // attention: need to switch in calling routine
    return state_mng_id_2_state(next_id);
} 


/**
 * @brief  Non thread-safe getter of state struct.
 *          Call ONLY from state_mng run loop.
 * 
 */
struct State * state_mng_id_2_state(cycle_state_id_t id_name){
    if(unlikely(id_name > _NUM_CYCLE_STATES || id_name == _NIL_CYCLE_STATE)){
        SYS_LOG_ERR("Requested invalid state_id: %i", id_name);
        return NULL;
    }
    return &(_states[id_name]);
}


/**
 *  @brief Read all events in driver queue and get the one of highest priority.
 * 
 *  If in STATE_IDLE, wait here FOREVER for first drv event.
 *  In case of event with same priority, the one later received (higher index in queue) wins.
 *
 *  @param res_evt: Pointer to state manager event to write result.
 */
static int gather_events(struct SMEvent * res_evt){
    struct DrvEvent drv_evt;                // driver event type
    res_evt->id_name = _CYCLE_DEFAULT_EVT;  // sm transition event type
    
    bool haveRx = false;
    short irq_high_prio = 0;
    short irq_cur_prio = 0;

 

    // reduces latency of spinning through NULL-event
    int k_wait = K_NO_WAIT;
    
    if(unlikely(_state_cur->id_name == CYCLE_STATE_IDLE)){
        int msgs_in_q = k_msgq_num_used_get(&queue_rx_driver);
        if(msgs_in_q == 0){
            //SYS_LOG_DBG("Going to block in STATE_IDLE, waiting for queue (%i msgs) events...", msgs_in_q);
            k_wait = K_FOREVER;
            //k_sleep(0);     // allow lower prio main thread to take over
        }
    }
    
    // listen to driver queue
    while(0 == k_msgq_get(&queue_rx_driver, &drv_evt, k_wait)){
        // after receiving first queue element (in STATE IDLE), we stop blocking
        k_wait = K_NO_WAIT;
        // filter all wanted IRQs
        if(likely(drv_evt.event_type == EVT_T_IRQ)){
            // only return highest prio event
            irq_cur_prio = drv_evt.prio;
            if(likely(irq_cur_prio >= irq_high_prio)){
                // transtale driver event to sm event
                // currently only sm event: reset on IRQ1
                if(likely(drv_evt.irq_id == IRQ_1))
                    res_evt->id_name = CYCLE_EVENT_RESET_IRQ;
                else
                    res_evt->id_name = _CYCLE_DEFAULT_EVT;
                irq_high_prio = irq_cur_prio;
            }
        }
        haveRx = true;
        //SYS_LOG_DBG("[%u] Received event of type %i from driver", state_mng_get_time_delta(), drv_evt.event_type);    
    }

    //if(state_mng_get_current() == CYCLE_STATE_IDLE)
    //    SYS_LOG_DBG("In state IDLE have_rx %i and chosen relevant state event id %i", haveRx, res_evt->id_name);    
       

    if(unlikely(!haveRx)){
        return 1;
    }

    /*
    if(res_evt->id_name != _CYCLE_DEFAULT_EVT)
        SYS_LOG_DBG("[%u] In state %i chosen relevant state event id %i", state_mng_get_time_delta(), state_mng_get_current(), res_evt->id_name);    
    */
    return 0;

}


static int state_mng_check_vals_ready(struct State * state){
    
    bool val_i_ready = false;
    
    for(int i=0; i<STATES_REQ_VALS_MAX; i++){
        // empty array entry means no more vals to check
        if(unlikely(state->val_ids_req[i] == _NIL_VAL))
            break;
        val_i_ready = irqtester_fe310_test_valflag(irqt_dev, state->val_ids_req[i]);

        if(unlikely(!val_i_ready)){
            // performance critical, do not print
            //SYS_LOG_WRN("Flag for requesting value %i from driver unready", state->val_ids_req[i]);
            return false;
        }
    }

    return true;
}

/**
 * @brief Wait until all requested values for state become available. 
 *         Timeout, if timing goal end of the state is hit.        
 *  
 * Also logs using waiting events if defined.
 * 
 * @return 0: all good. 1: timeout occured
 * 
 * Todo: currently busy waiting. Replace.
 */
static u32_t i_wait_evts;
bool state_mng_wait_vals_ready(struct State * state){
    
    bool timeout = false;
    bool ready = state_mng_check_vals_ready(state); // if already ready, don't enter loop
    bool waited = false;
    u8_t substate = state->cur_subs_idx;

    #if STATE_MNG_LOG_EVENTS_DEPTH > 0
        u32_t t_start_cyc = get_cycle_32();
    #endif

    while(!ready && !timeout){
        ready = state_mng_check_vals_ready(state);
        // todo: safety margin, such that timeout doesn't lead to fail of timing goal
        timeout = (state_mng_get_time_delta() > state_mng_get_timing_goal(state, substate, 1));
        waited = true;
    }

    #if STATE_MNG_LOG_EVENTS_DEPTH > 0
    if(waited){
        u32_t t_end_cyc = get_cycle_32();

        struct Wait_Event * w_evt_p = &(log_wait_evts[i_wait_evts % STATE_MNG_LOG_EVENTS_DEPTH]);
       
        w_evt_p->t_wait_cyc = t_end_cyc - t_start_cyc;
        w_evt_p->state = state->id_name;
        w_evt_p->substate = substate;
        w_evt_p->reason = RS_WAIT_FOR_VAL;
        i_wait_evts++; 
    }
    #endif

    if(timeout)
        return 1;
    
    return 0;
}

/**
 * @brief Wait for a fixed amount of time.
 * 
 * Also logs using waiting events if defined.
 *                   
 * @param t_fix_cyc: time to wait in cycles
 * @param reason: log the reason for waiting. Set 0 if no logging.
 * 
 * Todo: currently busy waiting. Replace.
 */
void state_mng_wait_fix(struct State * state, u32_t t_fix_cyc, sm_wait_reason_t reason){
    cycle_busy_wait(t_fix_cyc);

    #if STATE_MNG_LOG_EVENTS_DEPTH > 0
         // skip if state is not listed in filter
        if((!is_state_in_array(state->id_name, log_filter, LEN_ARRAY(log_filter))) \
            && log_filter[0] != _NIL_CYCLE_STATE)
            return;

        struct Wait_Event * w_evt_p = &(log_wait_evts[i_wait_evts % STATE_MNG_LOG_EVENTS_DEPTH]);
        u8_t substate = state->cur_subs_idx;
        
        w_evt_p->t_wait_cyc = t_fix_cyc;
        w_evt_p->state = state->id_name;
        w_evt_p->substate = substate;
        w_evt_p->reason = reason;
        i_wait_evts++; 
    
    #endif
}

/** 
 * @brief Handle if not all vals rady
 * 
 * @return handler decision to skip actions in run loop.
 */
static bool handle_vals_ready(struct State * state){
    // fire state callback if available
    if(state->handle_val_rfail != NULL)
        return state->handle_val_rfail(state);
    
    return false;
}

/** 
 *   @brief Calcs the time since start state.
 *   
 *   Warning: requires that delta to start state can never > 2^32 cycles. 
 *   @return delta in cpu cycles
 */ 
u32_t state_mng_get_time_delta(){
 
    return (get_cycle_32() - timestamp_reset);    
}

/**
 * @brief   Deprecated, use explicit _start, _end methods
 */
int state_mng_get_timing_goal(struct State * state, u8_t substate, int mode){
    
    int result = 0;
    
    switch(mode){
        case 0:
            result = state->timing_goal_start;
            break;
        case 1:
            result = state->timing_goal_end;
            break;
        default:
            SYS_LOG_ERR("Unknown mode %i", mode);
            result = -1;  
    }

    // substate logic
    // replicate duration of parent state + summand
    #if(STATES_DIS_SUBSTATES == 0)
    if(state->max_subs_idx > 0){
        result += substate * (state->timing_goal_end - state->timing_goal_start + state->timing_summand) ;
    }
    #endif

    return result;
}




/** 
 * Deprecated, use explicit _start, _end methods
 * @param t_left:   if goal met: delta in cycles still to goal 
 * @param mode:     0: check start goal, 1:end goal
 * @return          0: goal in future, 1: goal missed, 2: goal exactly hit
 */
/*
static int check_time_goal(struct State * state, int mode, int * t_left){

    *t_left = 0;

    if(state->id_name == CYCLE_STATE_IDLE){
        return 0;
    }

    u32_t goal = state_mng_get_timing_goal(state, state->cur_subs_idx, mode);
    u32_t delta_cyc = state_mng_get_time_delta();

    *t_left = goal - delta_cyc;

    // attention: delta and goal are both time differences since timestamp_reset 
    if(delta_cyc > goal){
        //SYS_LOG_WRN("Timing goal %i = %i cyc missed for state %i, now: %i", \
        //        mode, goal, state->id_name, delta_cyc);
        
        return 1;
    }  
    else if(delta_cyc < goal)
        return 0; 
    else
        return 2;
}
*/

/**
 * @brief Helper to be called from check_time_goal_start() or _end()
 */
static void calc_time_goal(struct State * state, int * t_left, int goal){
        
    u32_t delta_cyc = state_mng_get_time_delta();

    *t_left = goal - delta_cyc;

    // no "early return" for static branch prediction
    if(unlikely(state->id_name == CYCLE_STATE_IDLE)){
        *t_left = 0;
    }

}

/** 
 * @param t_left:   if goal met: delta in cycles still to goal 
 */
static void check_time_goal_start(struct State * state, int * t_left){

    u32_t goal = states_get_timing_goal_start(state, state->cur_subs_idx);
    calc_time_goal(state, t_left, goal);
}

/** 
 * @param t_left:   if goal met: delta in cycles still to goal 
 */
static void check_time_goal_end(struct State * state, int * t_left){

    u32_t goal = states_get_timing_goal_end(state, state->cur_subs_idx);
    calc_time_goal(state, t_left, goal);
}

/** 
 * Deprecated, use explicit _start, _end methods
 * @param t_left:   result from check_time_goal()
 * @param mode:     0: check start goal, 1:end goal
 */
/*
static void handle_time_goal(struct State * state, int mode, int t_left){
    // fire state callback if available
    switch(mode){
        case 0: 
            if(state->handle_t_goal_start != NULL)
                state->handle_t_goal_start(state, t_left);
            break;
        case 1:
             if(state->handle_t_goal_end != NULL)
                state->handle_t_goal_end(state, t_left);
            break;
        default:
            SYS_LOG_ERR("Unknown mode %i", mode);
    }
    
}
*/

/** 
 * @param t_left:   result from check_time_goal()
 */
static void handle_time_goal_start(struct State * state, int t_left){
    // fire state callback if available
    if(unlikely(state->handle_t_goal_start == NULL))
        return;
    state->handle_t_goal_start(state, t_left);       
}

/** 
 * @param t_left:   result from check_time_goal()
 */
static void handle_time_goal_end(struct State * state, int t_left){
    // fire state callback if available
    if(unlikely(state->handle_t_goal_end == NULL))
        return;
    state->handle_t_goal_end(state, t_left);  
}

/** 
 * @brief Measure duration of actions, after waiting in state is done.
 * 
 * Waiting occurs if a handler of state does so in case of missed time goal
 * or not available requested value
 * 
 * @param mode:     0: measurement start, 1: mes end 
 */
static void mes_perf(struct State * state, int mode){
#if STATE_MNG_LOG_EVENTS_DEPTH > 0
    static u32_t start_action; // in cyc
    static u32_t delta_action;
    static u32_t i_perf;  // attention: log corruption if overflows

    // skip if state is not listed in filter
    if((!is_state_in_array(state->id_name, log_filter, LEN_ARRAY(log_filter))) \
            && log_filter[0] != 0)
        return;

    switch(mode){
        case 0:
            start_action = state_mng_get_time_delta(); 
            break;
        case 1:
            delta_action = state_mng_get_time_delta() - start_action; 
            struct Perf_Event * evt_p;
            evt_p = &(log_perf_evts[i_perf % STATE_MNG_LOG_EVENTS_DEPTH]);
            evt_p->state = state->id_name;
            evt_p->substate = state->cur_subs_idx;
            evt_p->t_actions_cyc = delta_action;
            i_perf++;
            break;
        default:
            SYS_LOG_ERR("Unknown mode %i", mode);
    }
    
#endif
}

/**
 * @brief Empty event log
 */ 
static void state_mng_purge_evt_log(){

    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        struct Switch_Event empty = {.from_state = _NIL_CYCLE_STATE,
            .to_state = _NIL_CYCLE_STATE, .t_delta_cyc=0, .t_cyc=0};
        log_switch_evts[i] = empty;
    }

    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        struct Switch_Event empty = {.from_state = _NIL_CYCLE_STATE,
            .to_state = _NIL_CYCLE_STATE};
        log_switch_evts[i] = empty;
    }


    for(int i = 0; i<STATE_MNG_LOG_EVENTS_DEPTH; i++){
        struct Perf_Event empty = {.state = _NIL_CYCLE_STATE};
        log_perf_evts[i] = empty;
    }
}

/**
 * @brief Search whether val_id is in array.
 * @param irqt_val_id_t *:  array, assumed to be ordered, 
 *                          st. _NIL_VAL occurs only after any value
 */
static bool is_val_in_array(irqt_val_id_t val, irqt_val_id_t *arr, int len){
    int i;
    for (i=0; i < len; i++) {
        if(arr[i] == val)
            return true;
        if(arr[i] == _NIL_VAL)  
            return false;
    }
    return false;
}
/**
 * @brief Search whether cycle_state_id is in array.
 * @param irqt_val_id_t *:  array, assumed to be ordered, 
 *                          st. _NIL_VAL occurs only after any value
 */
static bool is_state_in_array(cycle_state_id_t state_id, cycle_state_id_t *arr, int len){
    int i;
    for (i=0; i < len; i++) {
        if(arr[i] == state_id)
            return true;
        if(arr[i] == _NIL_CYCLE_STATE)  
            return false;
    }
    return false;
}



/**
 *  @brief Fires all callbacks stored in internal array.
 * 
 *  Use state_manager::register_action() and ::purge_registered_action() to access.
 */
static void _default_action_dispatcher(cycle_state_id_t state){

    // created on stack, gets invalid after dispatching finished
    // holds pointer to current state of state_manager, content must not be
    // altered by a callback
    struct ActionArg arg ={.state_cur = state_mng_id_2_state(state_mng_get_current())};

    for(int i=0; i<STATES_CBS_PER_ACTION_MAX; i++){
        void (*cb)(struct ActionArg const *) = _cb_funcs[state][i];
        if(unlikely(cb == NULL)) return; // cbs are filled "bottom up" in register_action()
        else{
            // fire callback!
            cb(&arg);
        }
    }
}

#endif // TEST_MINIMAL