/**
 * @file
 * @brief Template file for creating state machines.
 * 
 * You might want to:
 * - define new actions. Keep track of users/nodes there
 * - define own irq handlers which deliver DrvEvents up to state_manager
 */


#include <zephyr.h>
#include "globals.h"
#include "states.h"
#include "state_manager.h"
#include "cycles.h"

/**
 * Define states for SM1
 * Currently, this is tightly coupled to cycle_state_id_t and cycle_event_id_t
 * declared in states.h
 * ----------------------------------------------------------------------------
 */
static struct State sm_t_idle 
= {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE};
static struct State sm_t_start 
= {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_END};
static struct State sm_t_end 
= {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE};

/**
 * Define transition table for SM1
 * First column: default event, is set automatically
 * ----------------------------------------------------------------------------
 */
static struct State sm_t_states[_NUM_CYCLE_STATES];
static cycle_state_id_t sm_t_tt[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];

// create thread
K_THREAD_STACK_DEFINE(thread_sm_t_stack, 3000);
struct k_thread thread_sm_t_data;
static int thread_sm_t_prio = -2;  // negative values are cooperative


void sm_t_fire_irqs(int period_irq1_us){
    // fire once to start running even if irq1 disabled
    u32_t num_irq_1 = (period_irq1_us == 0 ? 1 : UINT32_MAX);

    // program IRQ1 to fire periodically, set minimum for safety
    period_irq1_us = (period_irq1_us > 0 ? period_irq1_us : 1);
    u32_t period_1_cyc = CYCLES_US_2_CYC(period_irq1_us);

	struct DrvValue_uint reg_num = {.payload=num_irq_1};
	struct DrvValue_uint reg_period = {.payload=period_1_cyc};	

	irqtester_fe310_set_reg(g_dev_irqt, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(g_dev_irqt, VAL_IRQ_1_PERIOD, &reg_period);

    // start firing 
	irqtester_fe310_fire_1(g_dev_irqt);
}

/**
 * Define actions which are cbs called from state_mng_run()
 * ----------------------------------------------------------------------------
 */
static void action_hello(){
    printk("Hello from your FSM in state %i! \n", state_mng_get_current());
}

/**
 * Public functions: Set up, run SM and print diagnostics
 * ----------------------------------------------------------------------------
 */
void sm_t_run(){
    // init state array
    sm_t_states[CYCLE_STATE_IDLE] = sm_t_idle;
    sm_t_states[CYCLE_STATE_START] = sm_t_start;
    sm_t_states[CYCLE_STATE_END] = sm_t_end;
    // init transition table
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        sm_t_tt[i][CYCLE_EVENT_RESET_IRQ] = CYCLE_STATE_START;   
    }

    // pass sm_t config to state manager
    state_mng_configure(sm_t_states, (cycle_state_id_t *)sm_t_tt, _NUM_CYCLE_STATES, _NUM_CYCLE_EVENTS);
    
    // action fired in non-idle states
    state_mng_register_action(CYCLE_STATE_START, action_hello, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END, action_hello, NULL, 0);

    // create thread 
    k_thread_create(&thread_sm_t_data, thread_sm_t_stack,
                            K_THREAD_STACK_SIZEOF(thread_sm_t_stack), (void (*)(void *, void *, void *))state_mng_run,
                            NULL, NULL, NULL,
                            thread_sm_t_prio, 0, K_NO_WAIT);

    // prints only if logging turned on
    state_mng_print_state_config(); 
    state_mng_print_transition_table_config();

    state_mng_init(g_dev_irqt);

    if(0 != state_mng_start()){
        printk("Error while starting state_manager \n");
        return;
    }
    // caveat: time till IDLE state must be smaller than irq1 period
    // can only return control to main thread in IDLE state
    sm_t_fire_irqs(200000); // us

    k_sleep(200);  // ms
    state_mng_abort();

    // reset and stop firing
    irqtester_fe310_reset_hw(g_dev_irqt);
    // see (via logging) state_manager log
    state_mng_print_evt_log();
    // clean up sm
    state_mng_reset();

}