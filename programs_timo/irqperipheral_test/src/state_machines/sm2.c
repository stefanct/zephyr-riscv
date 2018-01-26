/**
 * Implementation of State Machine SM2.
 * For testing and benchmarkin with workload (actions).
 * 
 */

#include "sm_common.h"
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"
#include "sm2_tasks.h"
#include "sm1.h" // debug only

// ugly: todo remove driver pointer from public driver interface
struct device * g_dev_cp;


/**
 * Define states for SM2
 * Currently, this is tightly coupled to cycle_state_id_t and cycle_event_id_t
 * declared in states.h
 * ----------------------------------------------------------------------------
 */

static struct State sm2_idle 
    = {.id_name = CYCLE_STATE_IDLE,     .default_next_state = CYCLE_STATE_IDLE};
static struct State sm2_start 
    = {.id_name = CYCLE_STATE_START,    .default_next_state = CYCLE_STATE_DL_CONFIG};
static struct State sm2_dl_config 
    = {.id_name = CYCLE_STATE_DL_CONFIG,.default_next_state = CYCLE_STATE_DL};
static struct State sm2_dl 
    = {.id_name = CYCLE_STATE_DL,       .default_next_state = CYCLE_STATE_UL_CONFIG};
static struct State sm2_ul_config 
    = {.id_name = CYCLE_STATE_UL_CONFIG,.default_next_state = CYCLE_STATE_UL};
static struct State sm2_ul 
    = {.id_name = CYCLE_STATE_UL,       .default_next_state = CYCLE_STATE_RL_CONFIG};
static struct State sm2_rl_config
    = {.id_name = CYCLE_STATE_RL_CONFIG,.default_next_state = CYCLE_STATE_RL};
static struct State sm2_rl 
    = {.id_name = CYCLE_STATE_RL,   .default_next_state = CYCLE_STATE_END};
static struct State sm2_end 
    = {.id_name = CYCLE_STATE_END,  .default_next_state = CYCLE_STATE_IDLE};

// state array, init in run()
static struct State sm2_states[_NUM_CYCLE_STATES];


/**
 * Define transition table for SM2
 * First column (default event is set automatically)
 * Need only to define non-default events
 * ----------------------------------------------------------------------------
 */

static cycle_state_id_t sm2_tt[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];




/**
 * Define actions for SM2, are cbs called from state_mng_run()
 * see also sm2_tasks.h
 * ----------------------------------------------------------------------------
 */







/**
 * Helper config functions
 * ----------------------------------------------------------------------------
 */

static void config_handlers(){

    // timing handlers, checked in state_manager::check_time_goal
    sm2_dl_config.handle_t_goal_start = sm_com_handle_timing_goal_start; 
    sm2_dl_config.handle_t_goal_end = sm_com_handle_timing_goal_end;
    //sm2_dl.handle_t_goal_start = sm_com_handle_timing_goal_start; 
    sm2_dl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    //sm2_ul_config.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm2_ul_config.handle_t_goal_end = sm_com_handle_timing_goal_end;
    //sm2_ul.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm2_ul.handle_t_goal_end = sm_com_handle_timing_goal_end;
    //sm2_rl_config.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm2_rl_config.handle_t_goal_end = sm_com_handle_timing_goal_end;
    //sm2_rl.handle_t_goal_start = sm_com_handle_timing_goal_start;
    sm2_rl.handle_t_goal_end = sm_com_handle_timing_goal_end;
    sm2_end.handle_t_goal_end = sm_com_handle_timing_goal_end;

    // requested values handlers, checked in state_manager::state_mng_check_vals_ready()
    //sm2_ul.handle_val_rfail = sm_com_handle_fail_rval_ul;
}


// Attention: acts on states defined here, not states in sm2_states array
static void config_timing_goals(int period_irq1_us, int period_irq2_us, int num_substates){
    
    int t_irq1_cyc = 65 * period_irq1_us;
    int t_irq2_cyc = 65 * period_irq2_us;

    // all durations in cyc
    int frac_trx_config = 10;
    int t_cfg = t_irq1_cyc / frac_trx_config;   // duration of sum of all cfg states
    int t_trx = t_irq1_cyc - t_cfg;             // duration of sum of all rx/tx states

    int t_state_trx = t_trx / 3;             // duration of single ul, dl or rl
    int t_state_cfg = t_cfg / 3;             // duration of single ul_config, ...
    int t_substate = (num_substates == 0 ? 0 : t_state_trx / num_substates); 
        
    printk("State trx duration %i us / %i cyc \n", t_state_trx / 65, t_state_trx);   
    printk("State cfg duration %i us / %i cyc \n", t_state_cfg / 65, t_state_cfg);  
    printk("Substate trx duration %i us / %i cyc \n", t_substate / 65, t_substate);   

    // currently: assume substates only in UL
    sm2_dl_config.timing_goal_start = 0;    // 0 isn't handled, START state is before 
    sm2_dl_config.timing_goal_end = t_state_cfg;
    sm2_dl.timing_goal_start = sm2_dl_config.timing_goal_end;
    sm2_dl.timing_goal_end   = sm2_dl.timing_goal_start + t_state_trx;

    sm2_ul_config.timing_goal_start = sm2_dl.timing_goal_end;
    sm2_ul_config.timing_goal_end = sm2_ul_config.timing_goal_start + t_state_cfg;
    sm2_ul.timing_goal_start = sm2_ul_config.timing_goal_end;
    // if substates, end of first substate
    sm2_ul.timing_goal_end   = sm2_ul.timing_goal_start + (num_substates == 0 ? t_state_trx : t_substate);  
    int t_state_ul = sm2_ul.timing_goal_end - sm2_ul.timing_goal_start;

    sm2_rl_config.timing_goal_start = sm2_ul.timing_goal_start + (num_substates == 0 ? t_state_trx : num_substates * (t_state_ul));
    sm2_rl_config.timing_goal_end = sm2_rl_config.timing_goal_start + t_state_cfg;
    sm2_rl.timing_goal_start = sm2_rl_config.timing_goal_end;
    sm2_rl.timing_goal_end   = sm2_rl.timing_goal_start + t_state_trx;

    sm2_end.timing_goal_end = sm2_rl.timing_goal_end;

    // make sure last end is < T(protocol cycle)
    if(sm2_end.timing_goal_end > t_irq1_cyc)
        printk("WARNING: STATE_END timing_goal_end %i us > period_1 %i us \n", 
            sm2_end.timing_goal_end / 65, period_irq1_us);

     if(t_substate < t_irq2_cyc){
        printk("WARNING: substate duration %i us < period_2 %i us \n", 
            t_substate / 65, period_irq2_us);    
     }
}

/**
 * Public functions: Set up, run SM2 and print diagnostics
 * ----------------------------------------------------------------------------
 */


static int num_substates; // = num of batches
static int num_user_batch;
static int num_users;

void sm2_config(int users, int usr_per_batch){
    num_substates = users / usr_per_batch;
    num_user_batch = usr_per_batch;
    num_users = users;
    
    if(users % usr_per_batch != 0){
        num_substates += 1;
        printk("WARNING: Fraction num_users / usr_per_batch non-integer. Setting num_substates= %i\n ", num_substates);
       
    }
}

void sm2_run(struct device * dev, int period_irq1_us, int period_irq2_us, void (*ul_task)(void), int param, int pos_param){

    g_dev_cp = dev;

    print_dash_line();
    printk_framed("Now running state machine sm2");
    print_dash_line();
    printk("param: %i, pos_param: %i \n" \
           "period_1: %i us, %i cyc, period_2: %i us, %i cyc\n" \
           "num_users: %i, substates: %i, usr_per_batch: %i \n", 
            param, pos_param, period_irq1_us, 65*period_irq1_us,
            period_irq2_us, 65*period_irq2_us, num_users, num_substates, num_user_batch);

    // additional config to states defined above
    if(period_irq2_us != 0){
        config_timing_goals(period_irq1_us, period_irq2_us, num_substates);
    }
    config_handlers();
    states_configure_substates(&sm2_ul, num_substates, 0);

    // transfer into and init state array
    sm2_states[CYCLE_STATE_IDLE] = sm2_idle;
    sm2_states[CYCLE_STATE_START] = sm2_start;
    sm2_states[CYCLE_STATE_DL_CONFIG] = sm2_dl_config;
    sm2_states[CYCLE_STATE_DL] = sm2_dl;
    sm2_states[CYCLE_STATE_UL_CONFIG] = sm2_ul_config;
    sm2_states[CYCLE_STATE_UL] = sm2_ul;
    sm2_states[CYCLE_STATE_RL_CONFIG] = sm2_rl_config;
    sm2_states[CYCLE_STATE_RL] = sm2_rl;
    sm2_states[CYCLE_STATE_END] = sm2_end;
    // init transition table
    // all resets lead to start state
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        sm2_tt[i][CYCLE_EVENT_RESET_IRQ] = CYCLE_STATE_START;   
    }
    // pass sm2 config to state manager
    state_mng_configure(sm2_states, (cycle_state_id_t *)sm2_tt, _NUM_CYCLE_STATES, _NUM_CYCLE_EVENTS);


    // todo: in case of shared irq for > 1 substate:
    // need to check whether values have uninit value and set skip action then
    //state_mng_register_action(CYCLE_STATE_IDLE , sm1_print_cycles, NULL, 0);
    //state_mng_register_action(CYCLE_STATE_IDLE , state_mng_print_transition_table_config, NULL, 0);
    
    state_mng_register_action(CYCLE_STATE_IDLE , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, sm_com_check_last_state, NULL, 0);

    state_mng_register_action(CYCLE_STATE_DL   , sm_com_clear_valflags, NULL, 0);

    // simulate requesting of a value, which is cleared in STATE_UL and every substate
    irqt_val_id_t reqvals_ul[] = {VAL_IRQ_0_PERVAL};
    if(ul_task != NULL){
        state_mng_register_action(CYCLE_STATE_UL   , ul_task, reqvals_ul, 1);

        // config for sm2 tasks
        if(ul_task == sm2_task_bench_basic_ops){
            switch(pos_param){
                case 0:
                    sm2_config_bench(param, 0, 0);
                    break;
                case 1:
                    sm2_config_bench(0, param, 0);
                    break;
                case 2:
                    sm2_config_bench(0, 0, param);
                    break;
                default:
                    printk("Error: Unknown pos_param %i. Aborting.", pos_param);
                    return;
            }
        }
        if(ul_task == sm2_task_calc_cfo_1){
            sm2_config_user_batch(num_user_batch);
        }

    }
    state_mng_register_action(CYCLE_STATE_UL   , sm_com_clear_valflags, reqvals_ul, 1);
    
    state_mng_register_action(CYCLE_STATE_RL   , sm_com_clear_valflags, NULL, 0);

    //state_mng_register_action(CYCLE_STATE_START, sm_com_print_cycles, NULL, 0);

    //state_mng_register_action(CYCLE_STATE_START, sm_com_speed_up_after_warmup, NULL, 0);

    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_last_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_clear_status, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_check_val_updates, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END  , sm_com_update_counter, NULL, 0);
    //state_mng_register_action(CYCLE_STATE_END  , sm_com_print_perf_log, NULL, 0);
    

    // state config done, print
    state_mng_print_state_config();
    state_mng_print_transition_table_config();

    // replace generic isr with optimized handler
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_0);
    irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_0);

    // make state manager ready to start
    state_mng_init(dev);

    // start the sm thread, created in main()
	if(0 != state_mng_start()){
        printk("ERROR: Couldn't start sm2. Issue with thread. Aborting...");
        return;
    }


    // program IRQ1 and IRQ2 to fire periodically
    // todo: hw support for infinite repetitions?
    u32_t period_1_cyc = period_irq1_us * 65; // x * ~1000 us
    u32_t period_2_cyc = period_irq2_us * 65;


	struct DrvValue_uint reg_num = {.payload=UINT32_MAX};
	struct DrvValue_uint reg_period = {.payload=period_1_cyc};	

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


    printk("DEBUG: SM2 offhanding to state manager thread \n");
}

void sm2_print_report(){
    sm_com_print_report();
}

