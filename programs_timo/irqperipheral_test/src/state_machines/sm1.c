/**
 * Implementation of State Machine SM1.
 * See notes 20171218.
 * Mainly for tests and benchmarking (no workload). See SM1 for extension with tasks.
 * 
 */

#include "sm1.h"
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"
#include "sm_common.h"

// ugly: todo remove driver pointer from public driver interface
struct device * g_dev_cp;


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
// todo: could probably hold only pointers, since states themself are static
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
    if(sm_com_get_i_run() % 100 == 0 || sm_com_get_i_run() % 100 == 1)
        printk("SM1 run %u in state %i, action at cycle %u, rtc %u \n", sm_com_get_i_run(), state_id, t_cyc, rtc_cyc);


}

// invoke in STATE_START
static void sm1_speed_up_after_warmup(){
    if(sm_com_get_i_run() > 5)
        return;
    else if(sm_com_get_i_run() == 4){
        // speed up after reset a few times
        // give icache chance to fetch handlers
        struct DrvValue_uint reg_period;
        irqtester_fe310_get_reg(g_dev_cp, VAL_IRQ_1_PERIOD, &reg_period);
        u32_t before = reg_period.payload;

        reg_period.payload=period_1_after_warmup;
        irqtester_fe310_set_reg(g_dev_cp, VAL_IRQ_1_PERIOD, &reg_period);
        printk("DEBUG: Speed up period 1 %u -> %u cpu cyles written to IRQ_1 reg \n", before, period_1_after_warmup);
    }
}



// invoke in STATE_START STATE_END
static void sm1_save_time(){
    cycle_state_id_t state_id = state_mng_get_current();
    u32_t time_cyc;
    int i_save = sm_com_get_i_run() % SM1_TIMING_BUFFER_DEPTH;

    if(state_id == CYCLE_STATE_START){
        timing_cyc_start[i_save] = get_cycle_32();
    }
    if(state_id == CYCLE_STATE_END){
        time_cyc = state_mng_get_time_delta();
        timing_cyc[i_save] = time_cyc;
        timing_cyc_end[i_save] = get_cycle_32();
        // debug
         // print only 2 runs every 100 runs
        //if(sm_com_get_i_run() % 100 == 0 || sm_com_get_i_run() % 100 == 1)
        //    printk("Saving cycle time %u at %u cycles in run %u\n", time_cyc, get_cycle_32(), _i_sm_run);
    }  
}





/**
 * Handling callbacks unique to single states
 * ----------------------------------------------------------------------------
 */

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
    
    sm1_dl.handle_t_goal_start = sm_com_handle_timing_goal_start; 
    sm1_dl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_ul.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm1_ul.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_rl.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm1_rl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_end.handle_t_goal_end = sm_com_handle_timing_goal_end;
    
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




void sm1_run(struct device * dev, int period_irq1_us, int period_irq2_us){

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
        sm1_ul.handle_val_rfail = sm_com_handle_fail_rval_ul;
    }
    

    int substate_summand = 0;  // safety between substates, unnecessary
    states_configure_substates(&sm1_ul, num_substates, substate_summand);
    // transfer into and init state array
    sm1_states[CYCLE_STATE_IDLE] = sm1_idle;
    sm1_states[CYCLE_STATE_START] = sm1_start;
    sm1_states[CYCLE_STATE_DL] = sm1_dl;
    sm1_states[CYCLE_STATE_UL] = sm1_ul;
    sm1_states[CYCLE_STATE_RL] = sm1_rl;
    sm1_states[CYCLE_STATE_END] = sm1_end;
    // init transition table
    // all resets lead to start state
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        sm1_tt[i][CYCLE_EVENT_RESET_IRQ] = CYCLE_STATE_START;   
    }

    state_mng_configure(sm1_states, (cycle_state_id_t *)sm1_tt, _NUM_CYCLE_STATES, _NUM_CYCLE_EVENTS);
    state_mng_print_state_config();

    // todo: in case of shared irq for > 1 substate:
    // need to check whether values have uninit value and set skip action then

    /* printing only for debug
    state_mng_register_action(CYCLE_STATE_START, sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_DL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_UL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_RL   , sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_print_cycles, NULL, 0);
    */
    state_mng_register_action(CYCLE_STATE_IDLE , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm_com_check_last_state, NULL, 0);

    state_mng_register_action(CYCLE_STATE_DL   , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_DL   , sm_com_clear_valflags, NULL, 0);

    // simulate requesting of a value, which is cleared in STATE_UL and every substate
    irqt_val_id_t reqvals_ul[] = {VAL_IRQ_0_PERVAL};
    state_mng_register_action(CYCLE_STATE_UL   , sm_com_clear_valflags, reqvals_ul, 1);
    state_mng_register_action(CYCLE_STATE_UL   , sm_com_check_last_state, NULL, 0);
    
    state_mng_register_action(CYCLE_STATE_RL   , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_RL   , sm_com_clear_valflags, NULL, 0);

    //state_mng_register_action(CYCLE_STATE_START, sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm1_save_time, NULL, 0);
    //state_mng_register_action(CYCLE_STATE_START, sm1_speed_up_after_warmup, NULL, 0);

    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_clear_status, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_val_updates, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_update_counter, NULL, 0);
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
        sm_com_set_val_uptd_per_cycle(div_1_2);

        reg_period.payload = period_2_cyc;
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_PERIOD, &reg_period);
        irqtester_fe310_fire_2(dev);
    }

    printk("DEBUG: SM1 offhanding to state manager thread \n");

}

void sm1_print_report(){
    sm_com_print_report();

    printk("Detailed timing of last %i runs [cpu cycles]: \n", SM1_TIMING_BUFFER_DEPTH);
    printk("State START to END time: \n");
    printk("{[");
    print_arr_uint(timing_cyc, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    /*
    printk("Detailed timing_end  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(timing_cyc_end, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    printk("Detailed timing_start  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(timing_cyc_start, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");
    */
}

void sm1_reset(){
    sm_com_reset();
}
