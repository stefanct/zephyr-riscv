#include "sm2_tasks.h"
#include "state_manager.h"
#include "log_perf.h"
/*
 *  Current plans:
 *  - a batch shares a common irq and hw buffer registers
 *      and corresponds to a substate
 * 
 */

#define SM2_CFO_ARR_DEPTH 1
#define SM2_NUM_USRS_PER_BATCH 3

typedef enum{
    _NIL_USER,  // must be first
    USER_1,
    USER_2,
    USER_3,
    _NUM_USERS  // must be last
}sm2_user_id_t;

// substates: [0, state->max_subs_idx]
// num_in_batch: [0, SM2_NUM_USRS_PER_BATCH - 1]
static sm2_user_id_t state_2_userid(struct State * state, short substate, short num_in_batch){
    
    sm2_user_id_t user_id = _NIL_USER;

    // performance vs safety
    if(num_in_batch > SM2_NUM_USRS_PER_BATCH - 1 || substate > state->max_subs_idx){
        LOG_PERF("WARNING: invalid num_in_batch %u or substate id %u", num_in_batch, substate);
        return user_id;
    }

    switch(state->id_name){
        case CYCLE_STATE_UL:
            user_id = (num_in_batch + 1) + (substate * num_in_batch); // = 1: for 0,0
            break; 
        default:
            LOG_PERF("WARNING: unsupported state id %i", state);
            return _NIL_USER;
    }

    return user_id;
}



void sm2_task_config_dsps(){
    cycle_state_id_t state_id = state_mng_get_current();

    // todo: machanism to make sure this is done before
    // actual data is rx/tx in this state
    // wrong DSP config might corrupt data
    // eg. DL_ENTER state?

    switch(state_id){
        case CYCLE_STATE_DL:
            // stub, do DSP config for state
            break;
        case CYCLE_STATE_UL:
            break;
        case CYCLE_STATE_RL:
            break;
        default:
            ;
    }
}


// static cfo_results_log 

static short i_cfo_buf = 0;
// circular buffer of cfo data per user
static u32_t user_data_arr_cfo[_NUM_USERS - 1][SM2_CFO_ARR_DEPTH]; // mind NIL_USER
void sm2_task_calc_cfo_correction(){

    struct State * state_cur = state_mng_id_2_state(state_mng_get_current()); 
    u8_t substate = state_cur->cur_subs_idx;

    // values to load from driver for each user in batch
    // make sure that sm requests those! Otherwise no guarantee for sinsible payload
    // todo: dummy values. (write back to dsp3 which is not used in sm1)
    irqt_val_id_t read_reg[SM2_NUM_USRS_PER_BATCH]  = {VAL_IRQ_0_PERVAL, VAL_IRQ_0_PERVAL, VAL_IRQ_0_PERVAL};
    irqt_val_id_t write_reg[SM2_NUM_USRS_PER_BATCH] = {VAL_DSP_3_CLEAR_ID, VAL_DSP_3_CLEAR_ID, VAL_DSP_3_CLEAR_ID};

    // TODO: DUMMY FORMULA, use and update weight over all users to calc 
    for(int i = 0; i<SM2_NUM_USRS_PER_BATCH; i++){
        // find out user
        sm2_user_id_t user = state_2_userid(state_cur, substate, i);
        // read value from driver
        struct DrvValue_uint cfo;
        irqtester_fe310_get_val(read_reg[i], &cfo);
        u32_t cfo_val_in = cfo.payload;
        // CFO CALCULATION
        u32_t cfo_last_res = user_data_arr_cfo[user - 1][i_cfo_buf];   // 0: _NIL_USER

        // multiply accumulate
        u32_t cfo_val_res = cfo_last_res + user * cfo_val_in;
        // todo: statistics?        
        // write back
        user_data_arr_cfo[user - 1][i_cfo_buf] = cfo_val_res;
        cfo.payload = cfo_val_res;
        irqtester_fe310_set_reg(g_dev_cp, write_reg[i], &cfo);

    }
    // idx for cfo_arr
    i_cfo_buf++;
    if(i_cfo_buf > SM2_CFO_ARR_DEPTH - 1)
        i_cfo_buf = 0;

}

// do in END state, if enough time?
void sm2_task_print_cfo_stat(){

}