#include "sm_common.h"
#include <zephyr.h>
#include "cycles.h"
#include "state_manager.h"
#include "states.h"
#include "irqtestperipheral.h"
#include "utils.h"
#include "log_perf.h"


#ifndef TEST_MINIMAL


static u32_t num_val_updt_per_cycle;
static u32_t num_val_updt;

// todo: move into some sort of diagnostics struct
static atomic_t _i_sm_run;
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

/// thread-safe
u32_t sm_com_get_i_run(){
    return atomic_get(&_i_sm_run);
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
        atomic_inc(&_i_sm_run);
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
            LOG_PERF("WARNING: Expected %u new val updates in run %i, got: %u", num_val_updt_per_cycle, atomic_get(&_i_sm_run), num_irq2s - num_irq2s_stamp);
            num_fail_valupdt++;
        }

        num_val_updt += num_irq2s - num_irq2s_stamp;
        num_irq2s_stamp = num_irq2s;
    }
   
}

// check in START and END, IDLE
void sm_com_check_last_state(){
    static cycle_state_id_t state_id;
    cycle_state_id_t state_id_new = state_mng_get_current();
    
    // check whether reset to START was while not finshed a cycle yet
    if(state_id_new == CYCLE_STATE_START){
        if(unlikely(!(state_id == CYCLE_STATE_IDLE || state_id == CYCLE_STATE_END))){
            num_fail_reset++;
            //u32_t time_cyc = get_cycle_32();
            //u32_t time_delta_cyc = state_mng_get_time_delta();
            // printing is slow, use only for debug
            //printk("WARNING: [%u / %u] Reset before finsihing cycle in run %u. Last state was %i \n", time_delta_cyc, time_cyc, _i_sm_run, state_id);
            atomic_inc(&_i_sm_run);
          
            // allow other thread terminating sm
            // even if IDLE is not reached anymore
            k_yield(); 
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
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u", status_1.payload, status_2.payload, atomic_get(&_i_sm_run) );
        status_1_stamp = status_1.payload;
        num_fail_status_1++;
    }
    
    if(status_2.payload != status_2_stamp){
        LOG_PERF("WARNING: Non-zero irq1/2 status reg = %u, %u in run %u", status_1.payload, status_2.payload, atomic_get(&_i_sm_run));
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
    if(unlikely(t_left < 0 && state->timing_goal_start != 0)){
        LOG_PERF("WARNING: [%u] missed t_goal_start %u in state %i.%u",
                state_mng_get_time_delta(), state->timing_goal_start, state->id_name, state->cur_subs_idx);
        num_fail_timing++;
    }
    // todo: depending whether waiting on time (not reqval) is common, mark likely
    else if(t_left > 0 && state->timing_goal_start != 0){
        state_mng_wait_fix(state, t_left, RS_WAIT_FOR_START);   
    }
}

/// warn on miss end
void sm_com_handle_timing_goal_end(struct State * state, int t_left){
    if(unlikely(t_left < 0 && state->timing_goal_end != 0)){
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

void sm_com_print_perf_log(){
    // todo: this method may drop values or print them twice
    // in cause of ring buffer overflow

    // buffer should be bigger than expected results per state cycle
    // writting to uart is slow, so do only if buffer full
    #define buf_len 100
    static int buf[buf_len];    // ring buf
    static int idx_0 = 0;
    static int idx_1 = 0;  // guaranteed to stay < buf_len
    
    // deletes pulled actions from state_manager log
    idx_1 = state_mng_pull_perf_action(buf, idx_0, buf_len);

    // write to uart console (slow)
   
    if(idx_0 < idx_1){
        //print_arr_int(0, buf + idx_0, idx_1 - idx_0);
    }
    else if(idx_0 >= idx_1){
        // buffer full write out
        printk("Duration of last actions: \n");
        printk("{[");
        print_arr_int(0, buf, buf_len);    
        printk("]}");
    }
   
    idx_0 = idx_1;        
}

static u32_t stamp_perf_cache = 0;
static u32_t stamp_perf_branch = 0;
static short i_mes_perf = 0;
void sm_com_mes_mperf(){
    // those regs are platform specific, but zc706 and fe310 seem to work!
    #define FE310_PERF_CLASS_MICEVT 1
    #define FE310_PERF_CLASS_MEMEVT 2
    // instruction cache miss
    #define FE310_PERF_BITMASK_ICACHE_MISS (1 << 8)     // and class memevt
    // branch direction mispredictionch 
    #define FE310_PERF_BITMASK_BRANCH_MISP (1 << 13)    // and class micevt 
    // branch/jump target misprediction
    #define FE310_PERF_BITMASK_BRJMP_MISP (1 << 14)     // and class micevt


    // setup on first call
    if(i_mes_perf == 0){
        // configure mhpmevent3
        // st. branch misprediction counter in reg mhpmcounter3
        u32_t reg = FE310_PERF_CLASS_MICEVT;
        reg |= FE310_PERF_BITMASK_BRANCH_MISP | FE310_PERF_BITMASK_BRJMP_MISP;
        //reg |= 0xFFF00; // select all in class 1
        //reg = 1;

        __asm__ volatile("csrw mhpmevent3, %0" :: "r" (reg));
        // configure mhpmevent4
        // icache miss counter in reg mhpmcounter4
        u32_t reg2 = FE310_PERF_CLASS_MEMEVT;
        reg2 |= FE310_PERF_BITMASK_ICACHE_MISS;
        __asm__ volatile("csrw mhpmevent4, %0" :: "r" (reg2));

        // check whether activation successfull
        u32_t reg_check;
        u32_t reg2_check;
        __asm__ volatile("csrr %0, mhpmevent3" : "=r" (reg_check));
        __asm__ volatile("csrr %0, mhpmevent4" : "=r" (reg2_check));
        if(reg_check != reg || reg2_check != reg2){
            printk("WARNING: Failed activating performance monitoring. mhpe3, mhpe4: %u / %u\n", reg_check, reg2_check);
        }
    } 
    
    u32_t count_3 = 0;
    u32_t count_4 = 0;
    __asm__ volatile("csrr %0, mhpmcounter3" : "=r" (count_3));
    __asm__ volatile("csrr %0, mhpmcounter4" : "=r" (count_4));
    if(count_3 != 0 || count_4 != 0){
        LOG_PERF("[%u] %u icache miss, %u branch mispr. Total: %u / %u", 
                i_mes_perf, count_4 - stamp_perf_cache, count_3 - stamp_perf_branch,
                count_4, count_3);
        
        stamp_perf_branch = count_3;
        stamp_perf_cache = count_4;
    }
    
    i_mes_perf++;
}


/**
 * Others
 * ----------------------------------------------------------------------------
 */

void sm_com_reset(){
    atomic_set(&_i_sm_run, 0);
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

#endif TEST_MINIMAL