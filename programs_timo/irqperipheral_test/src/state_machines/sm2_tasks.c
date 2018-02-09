#include "sm2_tasks.h"
#include "state_manager.h"
#include "log_perf.h"
#include "sm2_tasks.h"
/*
 *  Current plans:
 *  - a batch shares a common irq and hw buffer registers
 *      and corresponds to a substate
 * 
 */

#define SM2_CFO_ARR_DEPTH 1
#define SM2_NUM_USRS_PER_BATCH_MAX 32

typedef enum{
    _NIL_USER,  // must be first
    USER_1,
    USER_2,
    USER_3,
    USER_4,
    USER_5,
    USER_6,
    USER_7,
    USER_8,
    USER_9,
    USER_10,
    USER_11,
    USER_12,
    USER_13,
    USER_14,
    USER_15,
    USER_16,
    USER_17,
    USER_18,
    USER_19,
    USER_20,
    USER_21,
    USER_22,
    USER_23,
    USER_24,
    USER_25,
    USER_26,
    USER_27,
    USER_28,
    USER_29,
    USER_30,
    USER_31,
    USER_32,
    _NUM_USERS  // must be last
}sm2_user_id_t;

static int num_usr_per_batch;
static int num_bench_macs;
static int num_bench_loads;
static int num_bench_writes;

void sm2_config_bench(int num_macs, int num_loads, int num_writes){
    num_bench_macs = num_macs;
    num_bench_loads = num_loads;
    num_bench_writes = num_writes;

    printk("Debug: Configuring task benchmark. num_macs: %i, num_loads: %i, num_writes: %i \n",
                num_macs, num_loads, num_writes);
}


void sm2_config_user_batch(int num_users){
    if(num_users > SM2_NUM_USRS_PER_BATCH_MAX){
        printk("Warning: Tried to set too hig users per batch %i, set to max %i\n", num_users, SM2_NUM_USRS_PER_BATCH_MAX);
        num_usr_per_batch = num_users;
    }

    num_usr_per_batch = num_users;

    printk("Debug: Configuring task. num_users_per batch: %i\n", num_users);
}

// substates: [0, state->max_subs_idx]
// num_in_batch: [0, SM2_NUM_USRS_PER_BATCH - 1]
static sm2_user_id_t state_2_userid(struct State * state, short substate, short num_in_batch){
    
    sm2_user_id_t user_id = _NIL_USER;

    // performance vs safety
    // only for verbosity, is catched in user_id check below
    if(num_in_batch > num_usr_per_batch - 1 || substate > state->max_subs_idx){
        printk("WARNING: invalid num_in_batch %u or substate id %u \n", num_in_batch, substate);
        return user_id;
    }

    switch(state->id_name){
        case CYCLE_STATE_UL:
            user_id = (num_in_batch + 1) + (substate * num_usr_per_batch); // = 1: for 0,0
            break; 
        default:
            printk("WARNING: unsupported state id %i \n", state->id_name);
            return _NIL_USER;
    }

    if(user_id < _NIL_USER || user_id >= _NUM_USERS){
        // occurs if num_in_batch is an empty user
        //printk("WARNING: invalid user_id %i set to _NIL_USER. In sm2_user_id_t? \n", user_id);
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


// circular buffer of cfo data per user
static u32_t user_data_arr_cfo[_NUM_USERS - 1][SM2_CFO_ARR_DEPTH]; // mind NIL_USER
static short i_cfo_buf = 0;

// simple cfo without considering all user weights
// per user in batch: 1 load, 1 save, 1 MAC
// must be called from non-preemtible task! (set_reg_fast)
void sm2_task_calc_cfo_1(struct ActionArg const * arg){

    struct State * state_cur = arg->state_cur; 
    u8_t substate = state_cur->cur_subs_idx;

    // values to load from driver for each user in batch
    // make sure that sm requests those! Otherwise no guarantee for sinsible payload
    // todo: dummy values. (write back to dsp3 which is not used in sm1)
    irqt_val_id_t read_reg  = VAL_IRQ_0_PERVAL;
    irqt_val_id_t write_reg = VAL_DSP_3_CLEAR_ID;
    
    // make sure that valid values are set for all possible users!
    //irqt_val_id_t read_reg[SM2_NUM_USRS_PER_BATCH_MAX]  = {VAL_IRQ_0_PERVAL, VAL_IRQ_0_PERVAL, VAL_IRQ_0_PERVAL};
    //irqt_val_id_t write_reg[SM2_NUM_USRS_PER_BATCH_MAX] = {VAL_DSP_3_CLEAR_ID, VAL_DSP_3_CLEAR_ID, VAL_DSP_3_CLEAR_ID};


    // TODO: DUMMY FORMULA, use and update weight over all users to calc 
    for(int i_usr = 0; i_usr<num_usr_per_batch; i_usr++){
        // find out user
        sm2_user_id_t user = state_2_userid(state_cur, substate, i_usr);
        // occurs if last substate not fully filled with user 
        if(user == _NIL_USER){
            break;
        }
        //printk("user %i for state %i.%u, batch_i %i \n", user, state_cur->id_name, substate, i_usr);
        // read value from driver
        
        struct DrvValue_uint cfo;
        // todo: own getters for uint / int / bool -> perf
        irqtester_fe310_get_val_uint(read_reg, &cfo);
        u32_t cfo_val_in = cfo.payload;
        
        // CFO CALCULATION
        u32_t cfo_last_res = user_data_arr_cfo[user - 1][i_cfo_buf];   // 0: _NIL_USER

        // multiply accumulate
        u32_t cfo_val_res = cfo_last_res + user * cfo_val_in;
        
        // todo: statistics?        
        // write back
        user_data_arr_cfo[user - 1][i_cfo_buf] = cfo_val_res;
        cfo.payload = cfo_val_res;
        // todo: own setters for uint / int / bool -> perf
        irqtester_fe310_set_reg_uint_fast(g_dev_cp, write_reg, &cfo);  
        
    }
    // idx for cfo_arr
    i_cfo_buf++;
    if(i_cfo_buf > SM2_CFO_ARR_DEPTH - 1)
        i_cfo_buf = 0;

}

// do in END state, if enough time?
void sm2_task_print_cfo_stat(){

}

// bench basic operation
void sm2_task_bench_basic_ops(){
    irqt_val_id_t read_reg  = VAL_IRQ_0_PERVAL;
    irqt_val_id_t write_reg = VAL_DSP_3_CLEAR_ID;
    int read_arr_irx = 0;
    
    struct DrvValue_uint val;
    u32_t val_in = 42;
    u32_t val_res = 7;

    for(int i=0; i < num_bench_loads; i++){
        irqtester_fe310_get_val_uint(read_reg, &val);
        val_in = val.payload;
        // test
        //val_in = irqtester_fe310_get_val_uint_raw_2(read_arr_irx);

    }
    for(int i=0; i < num_bench_macs; i++){
        val_res = i + i * val_in;
    }
    for(int i=0; i < num_bench_writes; i++){
        val.payload = val_res;
        irqtester_fe310_set_reg_uint_fast(g_dev_cp, write_reg, &val);
    }

}