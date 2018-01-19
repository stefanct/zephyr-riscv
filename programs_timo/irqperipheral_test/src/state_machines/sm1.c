/**
 * Implementation of State Machine SM1.
 * See notes 20171218
 * 
 */

#include "sm1.h"
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"

static struct device * dev_cp;


/**
 * Define states for SM1
 * Currently, this is tightly coupled to cycle_state_id_t and cycle_event_id_t
 * declared in states.h
 * ----------------------------------------------------------------------------
 */

static struct State sm1_idle = {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State sm1_start = {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_DL,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State sm1_dl = {.id_name = CYCLE_STATE_DL, .default_next_state = CYCLE_STATE_UL,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State sm1_ul = {.id_name = CYCLE_STATE_UL, .default_next_state = CYCLE_STATE_RL,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State sm1_rl = {.id_name = CYCLE_STATE_RL, .default_next_state = CYCLE_STATE_END,
        .timing_goal_start = 0, .timing_goal_end = 0,  
        };

static struct State sm1_end = {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE,
        .timing_goal_start = 0, .timing_goal_end = 0, 
        };

// state array, init in run()
static struct State sm1_states[_NUM_CYCLE_STATES];


/**
 * Define transition table for SM1
 * First column (default event is set automatically)
 * Need only to define non-default events
 * ----------------------------------------------------------------------------
 */

static cycle_state_id_t sm1_tt[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];



#define SM1_TIMING_BUFFER_DEPTH 100
static u32_t timing_cyc[SM1_TIMING_BUFFER_DEPTH];
static u32_t timing_cyc_start[SM1_TIMING_BUFFER_DEPTH];
static u32_t timing_cyc_end[SM1_TIMING_BUFFER_DEPTH];

static u32_t _i_sm_run;
static u32_t num_val_updt;
static u32_t num_val_updt_per_cycle;
static u32_t num_fail_valupdt;
static u32_t num_fail_reset;
static u32_t num_fail_status_1;
static u32_t num_fail_status_2;
static u32_t num_fail_timing;
static u32_t num_fail_reqval;

static u32_t period_1_after_warmup;



/**
 * Define actions for SM1, are cbs called from state_mng_run()
 * Mostly for diagnostics, see also sm1_tasks.h
 * ----------------------------------------------------------------------------
 */

static void sm1_print_cycles(){

    //printk("DEBUG: Entering sm1 action in run %i \n", _i_sm_run);
    cycle_state_id_t state_id = state_mng_get_current();
    u32_t t_cyc = get_cycle_32();
    u32_t rtc_cyc = k_cycle_get_32();

   
    // print only 2 runs every 100 runs
    if(_i_sm_run % 100 == 0 || _i_sm_run % 100 == 1)
        printk("SM1 run %u in state %i, action at cycle %u, rtc %u \n", _i_sm_run, state_id, t_cyc, rtc_cyc);


}

// invoke in STATE_START
static void sm1_speed_up_after_warmup(){
    if(_i_sm_run > 5)
        return;
    else if(_i_sm_run == 4){
        // speed up after reset a few times
        // give icache chance to fetch handlers
        struct DrvValue_uint reg_period;
        irqtester_fe310_get_reg(dev_cp, VAL_IRQ_1_PERIOD, &reg_period);
        u32_t before = reg_period.payload;

        reg_period.payload=period_1_after_warmup;
        irqtester_fe310_set_reg(dev_cp, VAL_IRQ_1_PERIOD, &reg_period);
        printk("DEBUG: Speed up period 1 %u -> %u cpu cyles written to IRQ_1 reg \n", before, period_1_after_warmup);
    }
}

// invoke in STATE_UL
static void sm1_clear_valflags(){
    // dummy load of value
    struct DrvValue_uint val;
    irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val);

    // use to differentiate behavior for differen substates
    //struct State * state_cur = state_mng_id_2_state(state_mng_get_current()); 
    //u8_t substate = state_cur->cur_subs_idx;

    // clear flag, such that requested in next substate again
    irqtester_fe310_clear_all_valflags(dev_cp);
}

// invoke in STATE_END
static void sm1_update_counter(){
    cycle_state_id_t state_id = state_mng_get_current();
    
    if(state_id == CYCLE_STATE_END)
        _i_sm_run++;
}

// invoke in STATE_START STATE_END
static void sm1_save_time(){
    cycle_state_id_t state_id = state_mng_get_current();
    u32_t time_cyc;
    int i_save = _i_sm_run % SM1_TIMING_BUFFER_DEPTH;

    if(state_id == CYCLE_STATE_START){
        timing_cyc_start[i_save] = get_cycle_32();
    }
    if(state_id == CYCLE_STATE_END){
        time_cyc = state_mng_get_time_delta();
        timing_cyc[i_save] = time_cyc;
        timing_cyc_end[i_save] = get_cycle_32();
        // debug
         // print only 2 runs every 100 runs
        //if(_i_sm_run % 100 == 0 || _i_sm_run % 100 == 1)
        //    printk("Saving cycle time %u at %u cycles in run %u\n", time_cyc, get_cycle_32(), _i_sm_run);
    }  
}


// check in STATE_END
static void sm1_check_val_updates(){
    cycle_state_id_t state_id = state_mng_get_current();
    static u32_t num_irq2s_stamp;

    //printk("DEBUG: Entering sm1 action in run %i \n", _i_sm_run);
    if(state_id == CYCLE_STATE_END){
        // is set in irq_2_handler_0 as simple counter
        // reset only on hardware reset, so use stamp to count
        // internal software variables
        struct DrvValue_uint perval;
        irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &perval);
        u32_t num_irq2s = perval.payload;
 
        if(num_irq2s - num_irq2s_stamp != num_val_updt_per_cycle){
            LOG_PERF("WARNING: Expected %u new val updates in run %i, got: %u", num_val_updt_per_cycle, _i_sm_run, num_irq2s - num_irq2s_stamp);
            num_fail_valupdt++;
        }

        num_val_updt += num_irq2s - num_irq2s_stamp;
        num_irq2s_stamp = num_irq2s;
    }
   
}

// check in all states
static void sm1_check_last_state(){
    static cycle_state_id_t state_id;
    cycle_state_id_t state_id_new = state_mng_get_current();

    // check whether reset to START was while not finshed a cycle yet
    if(state_id_new == CYCLE_STATE_START){
        if(!(state_id == CYCLE_STATE_IDLE || state_id == CYCLE_STATE_END)){
            num_fail_reset++;
            //u32_t time_cyc = get_cycle_32();
            //u32_t time_delta_cyc = state_mng_get_time_delta();
            // printing is slow, use only for debug
            //printk("WARNING: [%u / %u] Reset before finsihing cycle in run %u. Last state was %i \n", time_delta_cyc, time_cyc, _i_sm_run, state_id);
            _i_sm_run++;
            // count this aborted cycle
            k_yield(); // to allow other thread terminating sm
        }
    }

    state_id = state_id_new;
}

// check in STATE_END
static void sm1_check_clear_status(){
    static u32_t status_1_stamp;
    static u32_t status_2_stamp;

    struct DrvValue_uint status_1;
    struct DrvValue_uint status_2;
	irqtester_fe310_get_reg(dev_cp, VAL_IRQ_1_STATUS, &status_1);
    irqtester_fe310_get_reg(dev_cp, VAL_IRQ_2_STATUS, &status_2);


    if(status_1.payload != status_1_stamp){
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u \n", status_1.payload, status_2.payload, _i_sm_run);
        status_1_stamp = status_1.payload;
        num_fail_status_1++;
    }
    
    if(status_2.payload != status_2_stamp){
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u \n", status_1.payload, status_2.payload, _i_sm_run);
        status_2_stamp = status_2.payload;
        num_fail_status_2++;
    }

    
}




/**
 * Handling callbacks unique to single states
 * ----------------------------------------------------------------------------
 */

/// wait until start and warn on miss
static void handle_timing_goal_start(struct State * state, int t_left){
    if(t_left < 0 && state->timing_goal_start != 0){
        LOG_PERF("WARNING: [%u] missed t_goal_start %u in state %i.%u",
                state_mng_get_time_delta(), state->timing_goal_start, state->id_name, state->cur_subs_idx);
        num_fail_timing++;
    }
    else if(t_left > 0 && state->timing_goal_start != 0){
        state_mng_wait_fix(state, t_left, RS_WAIT_FOR_START);   
    }
}

/// warn on miss end
static void handle_timing_goal_end(struct State * state, int t_left){
    if(t_left < 0 && state->timing_goal_end != 0){
        num_fail_timing++;
        LOG_PERF("WARNING: [%u] missed t_goal_end %u in state %i.%u",
                state_mng_get_time_delta(), state->timing_goal_end, state->id_name, state->cur_subs_idx);
    }
}

static bool handle_fail_rval_ul(struct State * state_cur){
    

    // allows to ignore missing requested values for substates, eg.
    // if different substates share same irq / ready flag
    u8_t substate = state_cur->cur_subs_idx;

    bool timeout = state_mng_wait_vals_ready(state_cur);

    if(timeout){
        num_fail_reqval++;
        u32_t end = get_cycle_32();
        LOG_PERF("[%u / %u] Timeout requesting value in state %i.%u", 
                state_mng_get_time_delta(), end, state_cur->id_name, substate);
    }

    // slow, so only for debugging
    // LOG_PERF("[%u / %u] Waited %u cyc, state %i.%u", state_mng_get_time_delta(), end, end-start, state_cur->id_name, substate);
    
    
    return timeout;

}

/**
 * Helper config functions
 * ----------------------------------------------------------------------------
 */


// Attention: acts on states defined here, not states in sm1_states array
static void config_timing_goals(int period_irq1_us, int period_irq2_us, int num_substates){
    
    int period_irq1_cyc = 65 * period_irq1_us;
    int period_irq2_cyc = 65 * period_irq2_us;
    int t_safe_inter_states = 0;  // not needed, since we do not wait on end

    //sint num_tot_states = 3 + num_substates;
    int t_state = period_irq1_cyc / 3 - num_substates * t_safe_inter_states;  // duration of ul, dl or rl 
    int t_substate = t_state / num_substates + t_safe_inter_states; 
    
    sm1_dl.handle_t_goal_start = handle_timing_goal_start; 
    sm1_dl.handle_t_goal_end = handle_timing_goal_end;
    sm1_ul.handle_t_goal_start = handle_timing_goal_start;
    sm1_ul.handle_t_goal_end = handle_timing_goal_end;
    sm1_rl.handle_t_goal_start = handle_timing_goal_start;
    sm1_rl.handle_t_goal_end = handle_timing_goal_end;
    sm1_end.handle_t_goal_end = handle_timing_goal_end;
    
    printk("State duration %i us / %i cyc \n", t_state / 65, t_state);   
    printk("Substate duration %i us / %i cyc \n", t_substate / 65, t_substate);   

    // todo: check!
    sm1_dl.timing_goal_start = 0;
    sm1_dl.timing_goal_end = t_state;

    sm1_ul.timing_goal_start = sm1_dl.timing_goal_end + t_safe_inter_states;
    // end of first substate
    sm1_ul.timing_goal_end = sm1_ul.timing_goal_start + t_substate;
    int t_state_ul = sm1_ul.timing_goal_end - sm1_ul.timing_goal_start;

    sm1_rl.timing_goal_start = sm1_ul.timing_goal_start + num_substates * (t_state_ul + t_safe_inter_states);
    sm1_rl.timing_goal_end = sm1_rl.timing_goal_start + t_state;

    sm1_end.timing_goal_end = sm1_rl.timing_goal_end + t_safe_inter_states;

    // make sure last end is < T(protocol cycle)
    if(sm1_end.timing_goal_end > period_irq1_cyc)
        printk("WARNING: STATE_END timing_goal_end %i us > period_1 %i us \n", 
            sm1_end.timing_goal_end / 65, period_irq1_us);

     if(t_substate < period_irq2_cyc){
        printk("WARNING: substate duration %i us < period_2 %i us \n", 
            t_substate / 65, period_irq2_us);    
     }
}

/**
 * Public functions: Set up, run SM1 and print diagnostics
 * ----------------------------------------------------------------------------
 */

void sm1_print_state_config(){

    printk("SM1 State config: \n");

    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        struct State * state_cur = &(sm1_states[i]);
        int j = 0;
        do{
            printk("State %i.%u: t_goal_start= %u cyc (handler: %i), t_goal_end= %u cyc (handler: %i)\n",
                 state_cur->id_name, j, 
                 state_mng_get_timing_goal(state_cur, j, 0), 
                 state_cur->handle_t_goal_start != NULL, 
                 state_mng_get_timing_goal(state_cur, j, 1),
                 state_cur->handle_t_goal_end != NULL);
            j++;
        }while(j <= state_cur->max_subs_idx);
    }
}


void sm1_run(struct device * dev, int period_irq1_us, int period_irq2_us){

    // todo: find out why slow wehen only irq1
    // should be able to get to ~500 cycle without load and irq2=0

    dev_cp = dev;

    print_dash_line();
    printk_framed("Now running state machine sm1");
    print_dash_line();
    printk("period_1: %i us, %i cyc, period_2: %i us, %i cyc\n", 
            period_irq1_us, 65*period_irq1_us, period_irq2_us, 65*period_irq2_us);

    
    
    // additional config to states defined above
    int num_substates = 2;
    if(period_irq2_us != 0){
        config_timing_goals(period_irq1_us, period_irq2_us, num_substates);
        // todo: encapusulate this
        sm1_ul.handle_val_rfail = handle_fail_rval_ul;
    }
    
    // safety margin shouldn't be necessary: no wait on end, but on start
    // todo: verify and remove safety margins
    int substate_summand = 0; //(period_irq2_us == 0 ? 1 : 25 * 65); // safety between substates
    states_configure_substates(&sm1_ul, num_substates, substate_summand);
    // transfer into and init state array
    sm1_states[0] = sm1_idle;
    sm1_states[1] = sm1_start;
    sm1_states[2] = sm1_dl;
    sm1_states[3] = sm1_ul;
    sm1_states[4] = sm1_rl;
    sm1_states[5] = sm1_end;
    // init transition table
    // all resets lead to start state
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        sm1_tt[i][CYCLE_EVENT_RESET_IRQ] = CYCLE_STATE_START;   
    }

    sm1_print_state_config();

    state_mng_configure(sm1_states, (cycle_state_id_t *)sm1_tt, _NUM_CYCLE_STATES, _NUM_CYCLE_EVENTS);

    // todo: in case of shared irq for > 1 substate:
    // need to check whether values have uninit value and set skip action then

    /* printing only for debug
    state_mng_register_action(CYCLE_STATE_START, sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_DL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_UL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_RL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_print_cycles, NULL, 0);
    */
    state_mng_register_action(CYCLE_STATE_IDLE , sm1_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm1_check_last_state, NULL, 0);

    state_mng_register_action(CYCLE_STATE_DL   , sm1_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_DL   , sm1_clear_valflags, NULL, 0);

    // simulate requesting of a value, which is cleared in STATE_UL and every substate
    irqt_val_id_t reqvals_ul[] = {VAL_IRQ_0_PERVAL};
    state_mng_register_action(CYCLE_STATE_UL   , sm1_clear_valflags, reqvals_ul, 1);
    state_mng_register_action(CYCLE_STATE_UL   , sm1_check_last_state, NULL, 0);
    
    state_mng_register_action(CYCLE_STATE_RL   , sm1_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_RL   , sm1_clear_valflags, NULL, 0);

    //state_mng_register_action(CYCLE_STATE_START, sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm1_save_time, NULL, 0);
    //state_mng_register_action(CYCLE_STATE_START, sm1_speed_up_after_warmup, NULL, 0);

    state_mng_register_action(CYCLE_STATE_END  , sm1_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_check_clear_status, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_check_val_updates, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_update_counter, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_save_time, NULL, 0);

    // configure event passing driver -> sm
    // using default _irq_gen_handler
    state_mng_init(dev);
    // replace generic with optimized handler
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_0);
    irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_0);

	// start the sm thread, created in main()
	if(0 != state_mng_start()){
        printk("ERROR: Couldn't start sm1. Issue with thread. Aborting...");
        return;
    }


    // program IRQ1 and IRQ2 to fire periodically
    // todo: hw support for infinite repetitions?
    u32_t period_1_cyc = period_irq1_us * 65; // x * ~1000 us
    u32_t period_2_cyc = period_irq2_us * 65;

    // for first few runs, reset irq will be slower
    // to warm up icache
    // good idea, somehow not working currently... unstable timing irq1/2?
    period_1_after_warmup = period_1_cyc;
    //u32_t period_1_before_warmup = 10*period_1_cyc;
	struct DrvValue_uint reg_num = {.payload=UINT32_MAX};
	struct DrvValue_uint reg_period = {.payload=period_1_after_warmup};	

	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_PERIOD, &reg_period);

    // start firing reset irqs
	irqtester_fe310_fire_1(dev);

    // start firing val update irqs
    if(period_irq2_us != 0){
        int div_1_2 = period_irq1_us / period_irq2_us;    // integer division, p_1 always > p_2
        if(div_1_2 * period_irq2_us != period_irq1_us){
            printk("WARNING: Non integer divisble irq2 %u, irq1 %u lead to unstable timing relation. \n", period_irq2_us, period_irq1_us);
        }
        num_val_updt_per_cycle = div_1_2;

        reg_period.payload = period_2_cyc;
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_PERIOD, &reg_period);
        irqtester_fe310_fire_2(dev);
    }

    printk("DEBUG: SM1 offhanding to state manager thread \n");

}

void sm1_print_report(){
    printk("Report for sm1 out of %u runs \n", _i_sm_run);
    printk("Received value udpates [%u / %u] \n", num_val_updt, _i_sm_run * num_val_updt_per_cycle);
    printk("Failures: val_updt / reset / status1 / status2 / timing / reqval: %u, %u, %u, %u, %u, %u \n", \
             num_fail_valupdt, num_fail_reset, num_fail_status_1, num_fail_status_2, num_fail_timing, num_fail_reqval);
    printk("Detailed timing of last %i runs [cpu cycles]: \n", SM1_TIMING_BUFFER_DEPTH);
    printk("{[");
    print_arr_uint(timing_cyc, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    printk("Detailed timing_end  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(timing_cyc_end, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    printk("Detailed timing_start  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(timing_cyc_start, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");
}

void sm1_reset(){
    _i_sm_run = 0;
    num_val_updt = 0;

    num_fail_valupdt = 0;
    num_fail_reset = 0;
    num_fail_status_1 = 0;
    num_fail_status_2 = 0;
    num_fail_timing = 0;
    num_fail_reqval = 0;
}
