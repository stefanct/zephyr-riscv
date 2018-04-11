#include "sm1.h"
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"
#include "sm_common.h"
#include "globals.h"


//#define SM1_ENABLE
#ifdef SM1_ENABLE

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

void sm1_print_cycles(){

    //printk("DEBUG: Entering sm1 action in run %i \n", _i_sm_run);
    cycle_state_id_t state_id = state_mng_get_current();
    u32_t t_cyc = get_cycle_32();
    u32_t rtc_cyc = k_cycle_get_32();

   
    // print only 2 runs every 100 runs
    //if(sm_com_get_i_run() % 100 == 0 || sm_com_get_i_run() % 100 == 1)
        printk("SM1 run %u in state %i, action at cycle %u, rtc %u \n", sm_com_get_i_run(), state_id, t_cyc, rtc_cyc);


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

static void config_handlers(){

    // timing handlers, checked in state_manager::check_time_goal
    sm1_dl.handle_t_goal_start = sm_com_handle_timing_goal_start; 
    sm1_dl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_ul.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm1_ul.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_rl.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm1_rl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm1_end.handle_t_goal_end = sm_com_handle_timing_goal_end;

    // requested values handlers, checked in state_manager::state_mng_check_vals_ready()
    sm1_ul.handle_val_rfail = sm_com_handle_fail_rval_ul;
}



// Attention: acts on states defined here, not states in sm1_states array
static void config_timing_goals(int period_irq1_us, int period_irq2_us, int num_substates){
    
    int period_irq1_cyc = CYCLES_US_2_CYC(period_irq1_us);
    int period_irq2_cyc = CYCLES_US_2_CYC(period_irq2_us);
 
    //sint num_tot_states = 3 + num_substates;
    int t_state = period_irq1_cyc / 3 - num_substates;  // duration of ul, dl or rl 
    int t_substate = (num_substates == 0 ? 0 : t_state / num_substates); 

    printk("State duration %i us / %i cyc \n", CYCLES_CYC_2_US(t_state), t_state);   
    printk("Substate duration %i us / %i cyc \n", CYCLES_CYC_2_US(t_substate), t_substate);   

    // todo: check!
    sm1_dl.timing_goal_start = 0;
    sm1_dl.timing_goal_end = t_state;

    sm1_ul.timing_goal_start = sm1_dl.timing_goal_end;
    // end of first substate
    sm1_ul.timing_goal_end = sm1_ul.timing_goal_start + t_substate;
    int t_state_ul = sm1_ul.timing_goal_end - sm1_ul.timing_goal_start;

    sm1_rl.timing_goal_start = sm1_ul.timing_goal_start + num_substates * (t_state_ul);
    sm1_rl.timing_goal_end = sm1_rl.timing_goal_start + t_state;

    sm1_end.timing_goal_end = sm1_rl.timing_goal_end;

    // make sure last end is < T(protocol cycle)
    if(sm1_end.timing_goal_end > period_irq1_cyc)
        printk("WARNING: STATE_END timing_goal_end %i us > period_1 %i us \n", 
            CYCLES_CYC_2_US(sm1_end.timing_goal_end), period_irq1_us);

     if(t_substate < period_irq2_cyc){
        printk("WARNING: substate duration %i us < period_2 %i us \n", 
            CYCLES_CYC_2_US(t_substate), period_irq2_us);    
     }
}

/**
 * Public functions: Set up, run SM1 and print diagnostics
 * ----------------------------------------------------------------------------
 */




void sm1_run(struct device * dev, int period_irq1_us, int period_irq2_us){

    print_dash_line(0);
    printk_framed(0, "Now running state machine sm1");
    print_dash_line(0);
    printk("period_1: %i us, %i cyc, period_2: %i us, %i cyc\n", 
            period_irq1_us, CYCLES_US_2_CYC(period_irq1_us), period_irq2_us, CYCLES_US_2_CYC(period_irq2_us));

    
    
    // additional config to states defined above
    int num_substates = 2;
    if(period_irq2_us != 0){
        config_timing_goals(period_irq1_us, period_irq2_us, num_substates);
    }
    config_handlers();
    

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

    state_mng_register_action(CYCLE_STATE_DL   , sm_com_clear_valflags, NULL, 0);

    // simulate requesting of a value, which is cleared in STATE_UL and every substate
    irqt_val_id_t reqvals_ul[] = {VAL_IRQ_0_PERVAL};
    state_mng_register_action(CYCLE_STATE_UL   , sm_com_clear_valflags, reqvals_ul, 1);
   
    state_mng_register_action(CYCLE_STATE_RL   , sm_com_clear_valflags, NULL, 0);

    //state_mng_register_action(CYCLE_STATE_START, sm1_print_cycles, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm1_save_time, NULL, 0);
    //state_mng_register_action(CYCLE_STATE_START, sm1_speed_up_after_warmup, NULL, 0);

    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_clear_status, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_val_updates, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_update_counter, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm1_save_time, NULL, 0);

    // state config done, print
    state_mng_print_state_config();

    // replace generic isr with optimized handler
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_0);
    irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_0);

    // make state manager ready to start
    state_mng_init(dev);

	
    // program IRQ1 and IRQ2 to fire periodically
    // todo: hw support for infinite repetitions?
    u32_t period_1_cyc = CYCLES_US_2_CYC(period_irq1_us); // x * ~1000 us
    u32_t period_2_cyc = CYCLES_US_2_CYC(period_irq2_us);

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

    // start the sm thread, created in main()
	if(0 != state_mng_start()){
        printk("ERROR: Couldn't start sm1. Issue with thread. Aborting...");
        return;
    }

    printk("DEBUG: SM1 offhanding to state manager thread \n");

}

void sm1_print_report(){
    sm_com_print_report();

    printk("Detailed timing of last %i runs [cpu cycles]: \n", SM1_TIMING_BUFFER_DEPTH);
    printk("State START to END time: \n");
    printk("{[");
    print_arr_uint(0, timing_cyc, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    /*
    printk("Detailed timing_end  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(0, timing_cyc_end, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");

    printk("Detailed timing_start  [cpu cycles]: \n");
    printk("{[");
    print_arr_uint(0, timing_cyc_start, SM1_TIMING_BUFFER_DEPTH);
    printk("]}\n");
    */
}

void sm1_reset(){
    sm_com_reset();
    state_mng_reset();
}

#endif