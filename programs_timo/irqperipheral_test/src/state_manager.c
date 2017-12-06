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


enum{
    ABORT_LOOP,
    _NUM_FLAGS
};

atomic_t flags;


struct Event{
    cycle_event_id_t id_name;
};


// defines next state id_name on event
static cycle_state_id_t transition_table[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];

// lists all available states, must match cycle_state_id_t enum order
static struct State _states[_NUM_CYCLE_STATES];

atomic_t state_cur;                  // save to read from outside, use get_state()
static struct State * _state_cur;   // don't read from outside thread
static struct device * irqt_dev;
K_MSGQ_DEFINE(queue_rx_driver, sizeof(struct DrvEvent), STATE_MNG_QUEUE_RX_DEPTH, 4); // is static
static bool init_done = false;
static u32_t timestamp_reset;


// forward declare
static struct State * id_2_state(cycle_state_id_t id_name);

// PROBLEM: ZEPHYR SW TIMERS ON MS SCALE
// -> so far: no intrinsic hardware timers
// OWN HW TIMERS (COUNTER): 50 cycles to release mutex or queue msg
// -> measure systematic delay and counterbalance?





static void configure_states(void){
    // just to kepp implementation details of the specific states seperate
    // need to cast 2d array
    states_configure(_states, (cycle_state_id_t *)transition_table);
};



void state_mng_init(struct device * dev){
    if(dev != NULL){
        irqt_dev = dev;

        // setup irqtester device
        irqtester_fe310_register_queue_rx(dev, &queue_rx_driver);
        init_done = true;
    }
    else
        SYS_LOG_WRN("Can't set NULL as device");
}

void state_mng_abort(){
    atomic_set_bit(&flags, ABORT_LOOP);
}

cycle_state_id_t state_mng_get_current(){
    return atomic_get(&state_cur);
}

static struct State * switch_state(struct State * current, struct Event * evt){
    cycle_state_id_t next_id;
    cycle_event_id_t evt_id;

    if(evt == NULL)
        evt_id = _CYCLE_DEFAULT_EVT; // = 0: get first element of row
    else
        evt_id = evt->id_name;
    
    next_id = transition_table[current->id_name][evt_id];
    atomic_set(&state_cur, next_id); // to make avalable outside

    return id_2_state(next_id);
} 

static struct State * id_2_state(cycle_state_id_t id_name){
    return &(_states[id_name]);
}

/**
 * @brief Get priority of a DrvEvent
 * 
 * @return: priority, high positive values indicate high priority. -1: Error. Higher negative values: reserved.
 * 
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
 *  @brief Get all events in queue and decide which one is highest priority.
 * 
 *  In case of event with same priority, the one later received (higher index in queue) wins.
 */
static int gather_events(struct Event * res_evt){
    struct DrvEvent evt;

    bool haveRx = false;
    short irq_high_prio = 0;
    short irq_cur_prio = 0;

    // listen to driver queue
    while(0 == k_msgq_get(&queue_rx_driver, &evt, K_NO_WAIT)){

        // filter all wanted IRQs
        if(evt.event_type == IRQ){
            // only return highest prio event
            // currently indicated by highest int value irqt_event_type_t, todo
            irq_cur_prio = get_event_prio(&evt);
            if(irq_cur_prio >= irq_high_prio){
                res_evt->id_name = CYCLE_EVENT_RESET_IRQ;
                irq_high_prio = irq_cur_prio;
            }
        }
        haveRx = true;
        // SYS_LOG_DBG("Received event of type %i from driver", evt.event_type);    
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

/* 
    Calcs the time since start state.
    Warning: requires that delta to start state
    can never > 2^32 cycles. 
    @return: delta in cycles
*/ 
static u32_t get_time_delta(struct State * state){
 
    return (k_cycle_get_32() - timestamp_reset);    
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

// should be invoked as thread
void state_mng_run(void){
    
    if(!init_done){
        SYS_LOG_WRN("Can't start state machine before .init().");
        return;
    }
    configure_states();
    
    _state_cur = id_2_state(CYCLE_STATE_IDLE);
    // debug to start witouth irq
    _state_cur = id_2_state(CYCLE_STATE_START);
    struct Event event;
            

    irqtester_fe310_enable_queue_rx(irqt_dev);
    irqtester_fe310_enable_valflags_rx(irqt_dev);


    while(!atomic_test_bit(&flags, ABORT_LOOP)){
        // check other thread signals?
        
        if(_state_cur->id_name == CYCLE_STATE_IDLE){
            u32_t start_yield = k_cycle_get_32();
            k_yield();
            SYS_LOG_DBG("Yielded in IDLE state for %i cycles. Now sleep.", k_cycle_get_32() - start_yield);
            //k_sleep(100);
        }
        
        bool vals_ready; // move init out
        u32_t time_cyc_left;

        vals_ready = check_vals_ready(_state_cur);
        check_time_goal(_state_cur, 0, &time_cyc_left);
        // todo: handle missed vals or start time

        if(_state_cur->action != NULL)
            _state_cur->action();
        
        check_time_goal(_state_cur, 1, &time_cyc_left);
        // todo: check how much time left and wait?


        gather_events(&event);      // highest prio event during this state
        _state_cur = switch_state(_state_cur, &event);
    }


}