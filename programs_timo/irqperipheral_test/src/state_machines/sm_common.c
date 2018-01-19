#include "sm_common.h"
#include <zephyr.h>
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"




static u32_t _i_sm_run;
static u32_t num_val_updt_per_cycle;
static u32_t num_val_updt;


static u32_t num_fail_valupdt;
static u32_t num_fail_status_1;
static u32_t num_fail_status_2;
static u32_t num_fail_reset;
static u32_t num_fail_reqval;
static u32_t num_fail_timing;


/**
 * Getters, setters
 * ----------------------------------------------------------------------------
 */

u32_t sm_com_get_i_run(){
    return _i_sm_run;
}

void sm_com_set_val_uptd_per_cycle(int val){
    num_val_updt_per_cycle = val;

}


/**
 * Define actions used by multiple SMs
 * Mostly for diagnostics
 * ----------------------------------------------------------------------------
 */


// invoke in STATE_UL
void sm_com_clear_valflags(){
    // dummy load of value
    struct DrvValue_uint val;
    irqtester_fe310_get_val(VAL_IRQ_0_PERVAL, &val);

    // use to differentiate behavior for differen substates
    //struct State * state_cur = state_mng_id_2_state(state_mng_get_current()); 
    //u8_t substate = state_cur->cur_subs_idx;

    // clear flag, such that requested in next substate again
    irqtester_fe310_clear_all_valflags(g_dev_cp);
}

// invoke in STATE_END
void sm_com_update_counter(){
    cycle_state_id_t state_id = state_mng_get_current();
    
    if(state_id == CYCLE_STATE_END)
        _i_sm_run++;
}

// check in STATE_END
void sm_com_check_val_updates(){
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
void sm_com_check_last_state(){
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
void sm_com_check_clear_status(){
    static u32_t status_1_stamp;
    static u32_t status_2_stamp;

    struct DrvValue_uint status_1;
    struct DrvValue_uint status_2;
	irqtester_fe310_get_reg(g_dev_cp, VAL_IRQ_1_STATUS, &status_1);
    irqtester_fe310_get_reg(g_dev_cp, VAL_IRQ_2_STATUS, &status_2);


    if(status_1.payload != status_1_stamp){
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u", status_1.payload, status_2.payload, _i_sm_run);
        status_1_stamp = status_1.payload;
        num_fail_status_1++;
    }
    
    if(status_2.payload != status_2_stamp){
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u", status_1.payload, status_2.payload, _i_sm_run);
        status_2_stamp = status_2.payload;
        num_fail_status_2++;
    }

    
}



/**
 * Handling callbacks unique to single states
 * ----------------------------------------------------------------------------
 */

/// wait until start and warn on miss
void sm_com_handle_timing_goal_start(struct State * state, int t_left){
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
void sm_com_handle_timing_goal_end(struct State * state, int t_left){
    if(t_left < 0 && state->timing_goal_end != 0){
        num_fail_timing++;
        LOG_PERF("WARNING: [%u] missed t_goal_end %u in state %i.%u",
                state_mng_get_time_delta(), state->timing_goal_end, state->id_name, state->cur_subs_idx);
    }
}

bool sm_com_handle_fail_rval_ul(struct State * state_cur){
    

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
 * Others
 * ----------------------------------------------------------------------------
 */

void sm_com_reset(){
    _i_sm_run = 0;
    num_val_updt = 0;

    num_fail_valupdt = 0;
    num_fail_reset = 0;
    num_fail_status_1 = 0;
    num_fail_status_2 = 0;
    num_fail_timing = 0;
    num_fail_reqval = 0;
}

void sm_com_print_report(){
    printk("Report for sm_com out of %u runs \n", _i_sm_run);
    printk("Received value udpates [%u / %u] \n", num_val_updt, _i_sm_run * num_val_updt_per_cycle);
    printk("Failures: val_updt / reset / status1 / status2 / timing / reqval: %u, %u, %u, %u, %u, %u \n", \
             num_fail_valupdt, num_fail_reset, num_fail_status_1, num_fail_status_2, num_fail_timing, num_fail_reqval);


}