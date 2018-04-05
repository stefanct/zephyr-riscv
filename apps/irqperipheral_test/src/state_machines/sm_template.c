/**
 * @file
 * @brief Template file for creating state machines.
 * 
 */


#include <zephyr.h>
#include "globals.h"
#include "states.h"
#include "state_manager.h"
#include "cycles.h"

// debug
#include "log_perf.h"

// state machine definition
static struct State sm_t_idle 
= {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE};
static struct State sm_t_start 
= {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_END};
static struct State sm_t_end 
= {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE};

static struct State sm_t_states[_NUM_CYCLE_STATES];
static cycle_state_id_t sm_t_tt[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];

// create thread
K_THREAD_STACK_DEFINE(thread_sm_t_stack, 3000);
struct k_thread thread_sm_t_data;
static int thread_sm_t_prio = -2;  


void sm_t_fire_irqs(int period_irq1_us){
    // program IRQ1 and IRQ2 to fire periodically
    // todo: hw support for infinite repetitions?
    u32_t period_1_cyc = CYCLES_US_2_CYC(period_irq1_us); // x * ~1000 us

    // fire once to start running even if irq1 disabled
    u32_t num_irq_1 = (period_1_cyc == 0 ? 1 : UINT32_MAX);

	struct DrvValue_uint reg_num = {.payload=num_irq_1};
	struct DrvValue_uint reg_period = {.payload=period_1_cyc};	

	irqtester_fe310_set_reg(g_dev_irqt, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_set_reg(g_dev_irqt, VAL_IRQ_1_PERIOD, &reg_period);

    // start firing reset irqs
	irqtester_fe310_fire_1(g_dev_irqt);
}

static void action_hello(){
    printk("Hello from your FSM in state %i! \n", state_mng_get_current());
}

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


    state_mng_print_state_config(); // prints only if logging turned on
    state_mng_print_transition_table_config();
    state_mng_init(g_dev_irqt);

    if(0 != state_mng_start()){
        printk("Error while starting state_manager \n");
        return;
    }
    // caveat: time till IDLE state must be smaller than irq1 period
    // can only return control to main thread in IDLE state
    sm_t_fire_irqs(200000); // us

    k_sleep(1000);  // ms

    // reset and stop firing
    irqtester_fe310_reset_hw(g_dev_irqt);

    state_mng_abort();
    state_mng_print_evt_log();
    //PRINT_LOG_BUFF(1);
}