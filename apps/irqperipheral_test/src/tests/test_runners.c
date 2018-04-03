/**
 * @file
 * @brief Functions that bundle running different tests in tests.c 
 */



#include "test_runners.h"
#include "tests.h"
#include "utils.h"
#include "irqtestperipheral.h"
#include "state_manager.h"
#include "log_perf.h"
#include "state_machines/sm1.h"
#include "state_machines/sm2_tasks.h"
#include "state_machines/sm2.h"
#include "test_suite.h"
#include "cycles.h"
#include "profiler.h"

//#ifndef TEST_MINIMAL

/**
 * @brief: Basic checks for irqtestperipehral hardware. 
 */
int run_test_hw_basic_1(struct device * dev){
    
    // manually change for new hw rev
    printkv(2, "Hardware test for rev %i... \n", 3);    
    
    test_reset();   // deletes any test assert errors from before

    print_time_banner(2);
    printk_framed(2, "Now running basic hw test");
    print_dash_line(2);

    printk_framed(2, "Testing hw rev 1 functionality");  
    print_dash_line(2);
    
    test_hw_rev_1_basic_1(dev);

    test_print_report(2);  
    print_dash_line(2);
    printk_framed(2, "Testing hw rev 2 functionality");
        
    print_dash_line(2);
    
    test_hw_rev_2_basic_1(dev);

    test_print_report(2);
    print_dash_line(2);
    printk_framed(2, "Testing hw rev 3 functionality");
    print_dash_line(2);
    
    test_hw_rev_3_basic_1(dev);


    test_print_report(2);
    print_dash_line(2);
    test_print_report(2); // global var from tests.c

    
    int num_err = test_get_err_count();
    test_reset();

    printkv(1, "Hardware test for rev %i... ", 3);  
    if(num_err == 0)
        printkv(1, "ok \n");
    else
        printkv(1, "FAILED with %i errors\n", num_err);

    return num_err;
}


/**
 * @brief: Benchmark for differnt ways to implement up-pasing in ISRs.
 * Software measurement: Use for trends, not absolute numbers.
 * 
 * Note: on fe310, fifo is unstable. Hangs sometimes. 
 */
void run_test_timing_rx(struct device * dev){    
    #ifndef TEST_MINIMAL 

    const int NUM_RUNS = 10;		// warning high values may overflow stack
	int verbosity = 2;
	int timing_detailed_cyc[NUM_RUNS];
    test_reset();    // deletes any test assert errors from before

    print_banner(2);
    print_time_banner(2);
    

    printk_framed(2, "Now running raw irq to isr timing test");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, &irq_handler_mes_time);
    test_interrupt_timing(dev, timing_detailed_cyc, NUM_RUNS, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);

    //return;

    printk_framed(2, "Now running timing test with queues and direct reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0,_irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    printk_framed(2, "Now running timing test with queues and generic reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);
    
    printk_framed(2, "Now running timing test with fifos and direct reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with fifos and generic reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);

    printk_framed(2, "Now running timing test with semArr and direct reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with semArr and generic reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);
    
    printk_framed(2, "Now running timing test with valflags and direct reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with valflags and generic reg getters");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);

    printk_framed(2, "Now running timing test with hand-optimized valflag plus queue");
    print_dash_line(2);
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_5);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 4, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    // tests don't load values, so don't count errors here
    // -> reset error counter
    
    int errors_sofar = test_get_err_stamp();
    test_set_enable_print_warn(false);
    print_dash_line(2);
    printkv(2, "INFO: So far %i errors. Ignore the following error warnings for tests without load \n", errors_sofar);
    printk_framed(2, "Now running timing test with noload, queue and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with noload, fifos and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with noload, semArr and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed(2, "Now running timing test with noload, valflags and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);

    printk_framed(2, "Now running timing test with minimal, valflags\n");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_4);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(2);
    // reset counter
    test_set_err_count(errors_sofar);
    printkv(2, "INFO: End of noload. Resetting error count to %i \n", test_get_err_count());
    test_set_enable_print_warn(true);
    print_dash_line(2);

    // restore default handler
    irqtester_fe310_register_callback(dev, IRQ_0, _irq_gen_handler);
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);
    irqtester_fe310_register_callback(dev, IRQ_2, _irq_gen_handler);

    test_print_report(2); 

    printkv(1, "Timing rx test... ");  
    printkv(1, (test_get_err_count() == 0 ? "ok\n" : "failed\n"));
    #endif // TEST_MINIMAL
}

/// stripped down version of run_test_timing_rx
void run_test_min_timing_rx(struct device * dev){    
    
    const int NUM_RUNS = 100;		// warning high values may overflow stack
    int mode = 3;

	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    //int * timing_detailed_cyc = NULL;
    test_reset();

    switch(mode){
        case 4:
            printk_framed(0, "Now running timing test with hand-optimized valflag plus queue");
             irqtester_fe310_register_callback(dev, 0, _irq_0_handler_5);
            break;
        case 3:
            printk_framed(0, "Now running timing test with hand-optimized valflag only");
             irqtester_fe310_register_callback(dev, 0, _irq_0_handler_6);
            break;
        default:
            printk("ERROR: Not implemented \n");
            test_assert(0);
            return;
    }
 
   
    print_dash_line(0);
    // note: defining TEST_MINIMAL in driver sets callback in init function
    // might help compiler to optimize
   
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, mode, verbosity);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line(0);
    //
    //printk_framed(0, "Now running timing test with queues and direct reg getters");
    //print_dash_line(0);
    //irqtester_fe310_register_callback(dev, 0, _irq_gen_handler);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    //print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    //print_dash_line(0);

    test_print_report(0); // global var from tests.c

}

/**
 * @brief: easy irq throughput test, each irq period at a time.
 * 
 * - Can't observe cache warumup.
 * - Warning: inaccurate, period set for irqt is not exact
 *   better use chipscope
 */
void run_test_irq_throughput_1(struct device * dev){

    int num_runs = 10;

    // when choosing numbers, mind integer divison
    int start_dt_cyc = 2000;
    const int num_ts = 100;
    int cur_t_cyc = start_dt_cyc;
    int succ_t_cyc = start_dt_cyc;
    int dt_cyc = start_dt_cyc / num_ts;
    int t_low_cyc = 100; // safety to not freeze

    printk_framed(2, "Now running interrupt throughput test 1");
    print_dash_line(2);

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printkv(2, "Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);
    int status_stamp = 0;

    for(int i=0; i<num_ts; i++){
        int status = 0;
        test_irq_throughput_1(dev, cur_t_cyc, &status, num_runs);
        printkv(2, "From %i runs: dt_cyc / status / status_tot: {[%i, %i, %i ]}\n", num_runs, cur_t_cyc, status - status_stamp, status);
        if(status - status_stamp == 0)
            succ_t_cyc = cur_t_cyc;

        status_stamp = status;
        cur_t_cyc -= dt_cyc;
        if(cur_t_cyc <= t_low_cyc) 
            break;

    }

    printkv(1, "IRQ troughput test 1... ");  
    printkv(1, "%i cyc \n", succ_t_cyc);
    print_dash_line(2);

}

/**
 *  @brief: Advanced irq throughput test. 
 *          Repeat same irq period to see cache warumup.
 *
 * - Checks whether whole array has OK status (==0)
 * - Warning: inaccurate, period set for irqt is not exact
 *   better use chipscope
 */
void run_test_irq_throughput_2(struct device * dev){

    int num_runs = 20; // should equal to tests::STATUS_ARR_LEN
    const int status_arr_len = 20;

    // when choosing numbers, mind integer divison
    int num_ts = 400;
    int cur_t_cyc = 5000;
    int dt_cyc = 25;
    int t_low_cyc = 100; // safety to not freeze
    int t_succ_cyc = cur_t_cyc;

      
    printk_framed(2, "Now running interrupt throughput test 2");
    print_dash_line(2);

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printkv(2, "Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);


    int status_arr[status_arr_len];
    for(int i=0; i<status_arr_len; i++) status_arr[i] = -1;

    for(int i=0; i<num_ts; i++){

        test_irq_throughput_2(dev, cur_t_cyc, num_runs, status_arr, status_arr_len);
        printkv(2, "From %i runs with irq1 period %u cpu cycles. \n", num_runs, cur_t_cyc);

        printkv(2, "Detailed missed clear status per run \n");
	    print_arr_int(2, status_arr, status_arr_len);

        // check all repetitions status = 0
        for(int j=0; j<num_runs; j++){
            if(status_arr[j] != 0)
                break;
            if(j==num_runs-1)
                t_succ_cyc = cur_t_cyc;
        }

        cur_t_cyc -= dt_cyc;
        if(cur_t_cyc <= t_low_cyc) 
            break;

    }

    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printkv(2, "Status_1 after run: %i.  \n", status_1.payload);

    printkv(1, "IRQ troughput test 2... ");  
    printkv(1, "%i cyc \n", t_succ_cyc);
    print_dash_line(2);
}

/**
 * @brief: Advanced irq throughput test, adjusts itself to find minimum
 * 
 * - Warning: inaccurate, period set for irqt is not exact
 *   better use chipscope
 * - To deal with cache warmup effect, only check whether status ok (==0)
 *   for last repetition at a given irq period.
 */
int run_test_irq_throughput_3_autoadj(struct device * dev){
    
    int num_runs = 20; // runs per t
    u32_t guess_t_cyc = 1000;
    u32_t delta_cyc = 50;   // num_ts * delta_cyc should be enough to get close to 0
    int num_ts = 200;

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
    
    
    printkve(1, "IRQ throughput test 3 (autoadj)...");
    
    print_dash_line(2);
    printk_framed(2, "Now running interrupt throughput test 3 (auto adjust) for irq1 ");
    print_dash_line(2);
    printkv(2, "Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);
    
    int status_arr_len = num_runs;
    int status_arr[status_arr_len];
    int status_arr_success[status_arr_len];

    u32_t cur_t_cyc = guess_t_cyc;
    u32_t succes_t_cyc = 0;
    for(int i=0; i<num_ts; i++){
        // clear array
        for(int j=0; j<status_arr_len; j++)
            status_arr[j] = -1;
        test_irq_throughput_2(dev, cur_t_cyc, num_runs, status_arr, status_arr_len);
        // do very basic adjustment algorithm
        if(status_arr[status_arr_len - 1] != 0){
            // couln't clear interrupt in time, even after warming up icache
            if(succes_t_cyc == 0) {
                // haven't beeen successfull so far
                cur_t_cyc += delta_cyc;  
                delta_cyc *= 2;
            }
            else if(cur_t_cyc == succes_t_cyc){
                succes_t_cyc++;    // catch: doesn't work 2nd time
                cur_t_cyc = succes_t_cyc; 
            }
            else{
                 // go back to already sucessfull
                cur_t_cyc = succes_t_cyc;    
            }
            
        }
        else{
       
            bool overwrite_line = false;
            // overwrite same last result for verbosity 2 only
            if(print_get_verbosity() == 2){
                if(cur_t_cyc == succes_t_cyc)
                    overwrite_line = true;
                if(overwrite_line)
                    printkv(3, "\r");   // overwrite console
                else
                    printkv(3, "\n");
            }
            printkv(3, "[%i] Cleared interrupt with period t= %u cyc, t= %u us => f= %u kHz", i, cur_t_cyc, CYCLES_CYC_2_US(cur_t_cyc), 1000 / CYCLES_CYC_2_US(cur_t_cyc) ); 

            printkv(4, "\n");
            printkv(4, "Status array: \n");
            print_arr_int(4, status_arr, status_arr_len);
            
            

            succes_t_cyc = cur_t_cyc;
            
            if(num_runs > 2){
                if(status_arr[2] != 0){   
                    // means we're close too threshold
                    delta_cyc /= 2;
                }  
                else{
                    delta_cyc *= 2;
                }
            }

            // change cur_t and catch bad cases
            if(delta_cyc == 0)
                delta_cyc = 1;

            if(cur_t_cyc > delta_cyc)
                cur_t_cyc -= delta_cyc;

            for(int j=0; j<status_arr_len; j++)
                status_arr_success[j] = status_arr[j];
        }
        //printk("Debug: Changing cur_t_cyc= %u, , delta_t= %u \n", cur_t_cyc, cur_t_cyc);
    }

    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
    
 
    printkve(1, " t= %i cyc, f= %i kHz, status_1: %i.\n", 
             succes_t_cyc, 1000 / CYCLES_CYC_2_US(succes_t_cyc), status_1.payload);
    

    printkv(2, "\nFinished with irq t= %i cyc, f= %i kHz. Status_1 after run: %i.  \n",
            succes_t_cyc, 1000 / CYCLES_CYC_2_US(succes_t_cyc), status_1.payload);
    printkv(2, "Last cache warmup status_1 behaviour: \n");
    print_arr_int(2, status_arr_success, status_arr_len);
    print_dash_line(2);
  
    

    return succes_t_cyc;
    
}

/**
 * @brief: Don't use irqs, try to clear special hw block by polling.
 *  
 */
void run_test_poll_throughput_1_autoadj(struct device * dev){
    
    int num_runs = 20; // runs per t
    u32_t guess_t_cyc = 5000;
    u32_t delta_cyc = 50;   // num_ts * delta_cyc should be enough to get close to 0
    int num_ts = 250;

    printk_framed(2, "Now running poll throughput test 1 with auto adjust");
    print_dash_line(2);

    struct DrvValue_uint status_3;
	irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);
	printk("Status_3 before first run: %i. Test may take some seconds... \n", status_3.payload);

    int status_arr_len = num_runs;
    int status_arr[status_arr_len];
    int status_arr_success[status_arr_len];

    u32_t cur_t_cyc = guess_t_cyc;
    u32_t succes_t_cyc = 0;
    for(int i=0; i<num_ts; i++){
        test_poll_throughput_1(dev, cur_t_cyc, num_runs, status_arr, status_arr_len);
        // do very basic adjustment algorithm
        if(status_arr[status_arr_len - 1] != 0){
            // couln't clear in time, even after warming up icache
            if(succes_t_cyc == 0) {
                // haven't beeen successfull so far
                cur_t_cyc += delta_cyc;  
                delta_cyc *= 2;
            }
            else if(cur_t_cyc == succes_t_cyc){
                succes_t_cyc++;    // catch: doesn't work 2nd time
                cur_t_cyc = succes_t_cyc; 
            }
            else{
                 // go back to already sucessfull
                cur_t_cyc = succes_t_cyc;    
            }
            
        }
        else{
            if(cur_t_cyc == succes_t_cyc)
                printkv(2, "\r");   // overwrite console
            else
                printkv(2, "\n");
            printkv(2, "[%i] Cleared poll dev with period t= %u cyc, t= %u us => f= %u kHz", i, cur_t_cyc, CYCLES_CYC_2_US(cur_t_cyc), 1000 / CYCLES_CYC_2_US(cur_t_cyc)); 
            
            printkv(2, "\nCache warmup status_3 behaviour: \n");
            print_arr_int(2, status_arr, status_arr_len);

            succes_t_cyc = cur_t_cyc;
            
            if(status_arr[1] != 0){
                // means we're close to threshold
                delta_cyc /= 2;
            }  
            else{
                delta_cyc *= 2;
            }

            if(delta_cyc == 0)
                delta_cyc = 1;

            // change cur_t and catch bad cases 
            if(cur_t_cyc > delta_cyc)
                cur_t_cyc -= delta_cyc;

            for(int j=0; j<status_arr_len; j++)
                status_arr_success[j] = status_arr[j];
        }
       // printk("Debug: Changing cur_t_cyc= %u, delta_t= %u \n", cur_t_cyc, delta_cyc);
    }

    irqtester_fe310_get_reg(dev, VAL_DSP_3_STATUS, &status_3);
	printkv(2, "\nFinished, status_3 after run: %i.  \n", status_3.payload);
    printkv(2, "Last cache warmup status_1 behaviour: \n");
    print_arr_int(2, status_arr_success, status_arr_len);
    print_dash_line(2);

    printkv(1, "Polling test 1... ");  
    printkv(1, "%i cyc \n", succes_t_cyc);
}

// set value register to sum of states since START
// so while testing, after every fire() when returning back to IDLE
// state_sum is the sum of state_ids collected when running through the cycle
// we can assert on this
static struct device * dev_testscope;
void action_test_mng_1(struct ActionArg const * arg){
    static int state_sum;
    cycle_state_id_t state = arg->state_cur->id_name;
    // getting substate is save here, because fired out of state_mng loop
    u8_t subs = arg->state_cur->cur_subs_idx;

    if(unlikely(state == CYCLE_STATE_START)){
        state_sum = 0;
    }
    state_sum += state;

    // ok to print in action, shouln't be taken if verbosity low
    printkv(3, "Test action fired in state %i.%u, state_sum: %i \n", state, subs, state_sum);

    struct DrvValue_uint setval = {.payload=state_sum};

    irqtester_fe310_set_reg(dev_testscope, VAL_IRQ_0_VALUE, &setval);
    irqtester_fe310_clear_1(dev_testscope);

}


/**
 * @brief: Test of state_manager with autoconf SM
 * 
 */
K_THREAD_STACK_DEFINE(thread_mng_run_1_stack, 2048);
struct k_thread thread_mng_run_1_data;
void run_test_state_mng_1(struct device * dev){
   
    //todo: include substate logic in test
    int num_runs = 10;	
    dev_testscope = dev;
    int thread_mng_run_1_prio = -2;

    print_dash_line(2);
    printk_framed(2, "Now running state manager test 1");
    print_dash_line(2);

    // state_manager must to run at lower prio than main thread to return
    // (mind numerically lower values have high prio)
    if(CONFIG_MAIN_THREAD_PRIORITY >= thread_mng_run_1_prio){
        test_assert(0);
        printkv(1, "Thread prio (main: %i >= sm: %i) misconfigured.\n", CONFIG_MAIN_THREAD_PRIORITY, thread_mng_run_1_prio);
        return;
    } 
    // auto config, todo: make custom config for this test
    // in order to reduce coupling to states::auto implementation
    state_mng_configure(NULL, NULL, 0, 0);

    state_mng_register_action(CYCLE_STATE_IDLE, action_test_mng_1, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, action_test_mng_1, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END, action_test_mng_1, NULL, 0);

    // irq handler to send up reset event
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_0);
    state_mng_init(dev);

    // for debug state config done, print
    //state_mng_print_state_config();
    //state_mng_print_transition_table_config();

	// start the thread immediatly
	k_tid_t my_tid = k_thread_create(&thread_mng_run_1_data, thread_mng_run_1_stack,
                        K_THREAD_STACK_SIZEOF(thread_mng_run_1_stack), (void (*)(void *, void *, void *)) state_mng_run,
                        NULL, NULL, NULL,
                        thread_mng_run_1_prio, 0, K_NO_WAIT);
    printkv(2, "state_manager thread @ %p started\n", my_tid);

    test_assert(state_mng_start() == 0);

    // expection for automatic sm config
    int sum_expect = CYCLE_STATE_IDLE + CYCLE_STATE_END + CYCLE_STATE_START;
    for(int i=0; i<num_runs; i++){
        // fire (and run a state achine cycle) only every 100ms 
        k_sleep(100); //ms
        test_state_mng_1(dev, sum_expect);
    }
    
    /*
    k_sleep(100);   // hand of
    // fire irq1
    struct DrvValue_uint reg_num = {.payload=1};
	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
	irqtester_fe310_fire_1(dev);

    k_sleep(100);   // hand of
    */
    state_mng_abort();
    state_mng_purge_registered_actions_all();
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);


    // print for dbg
    //state_mng_print_evt_log();
    //test_print_report(0);

    int num_err = test_get_err_count();
    test_reset();

    // manually change for new hw rev
    printkv(1, "SM test (auto conf)... ");    
    if(num_err == 0)
        printkv(1, "ok \n");
    else
        printkv(1, "FAILED with %i errors\n", num_err);

}


/**
 * @brief: Benchmark time to run from reset once trough auto conf sm ("critical loop").
 */
void run_test_state_mng_2(struct device * dev){
   
    #include "../state_machines/sm_common.h"
    //todo: include substate logic in test
    const int num_runs = 10;     // high values may silently overflow stack!
    const int num_states = 3;    // see state_manager auto config 	
    dev_testscope = dev;
    int thread_mng_run_1_prio = -2;
    struct Switch_Event log_buf[num_states * num_runs + 1];     // must be big enough to hold event log
    int save_err = test_get_err_count();
    
    print_dash_line(2);
    printk_framed(2, "Now running state manager test 2");
    print_dash_line(2);

    // state_manager must to run at lower prio than main thread to return
    // (mind numerically lower values have high prio)
    if(CONFIG_MAIN_THREAD_PRIORITY >= thread_mng_run_1_prio){
        test_assert(0);
        printkv(0, "WARNING: Thread prio (main: %i >= sm: %i) misconfigured.\n", CONFIG_MAIN_THREAD_PRIORITY, thread_mng_run_1_prio);
        return;
    } 

    // auto config, todo: make custom config for this test
    // in order to reduce coupling to states::auto implementation
    state_mng_configure(NULL, NULL, 0, 0);

    state_mng_register_action(CYCLE_STATE_IDLE, action_test_mng_1, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, action_test_mng_1, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END, action_test_mng_1, NULL, 0);
    // only activate for debug, not benchmark
    //state_mng_register_action(CYCLE_STATE_END, sm_com_mes_mperf, NULL, 0);

    // irq handler to send up reset event
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_1_handler_0);
    state_mng_init(dev);

    // for debug state config done, print
    //state_mng_print_state_config();
    //state_mng_print_transition_table_config();

    // set log filter to STATE_IDLE
    state_mng_add_filter_state(CYCLE_STATE_IDLE);

	// start the thread immediatly
	k_tid_t my_tid = k_thread_create(&thread_mng_run_1_data, thread_mng_run_1_stack,
                        K_THREAD_STACK_SIZEOF(thread_mng_run_1_stack), (void (*)(void *, void *, void *)) state_mng_run,
                        NULL, NULL, NULL,
                        thread_mng_run_1_prio, 0, K_NO_WAIT);
    printkv(3, "state_manager thread @ %p started\n", my_tid);

    test_assert(state_mng_start() == 0);

    // expection for automatic sm config
    int sum_expect = CYCLE_STATE_IDLE + CYCLE_STATE_END + CYCLE_STATE_START;
    for(int i=0; i<num_runs; i++){
        test_state_mng_1(dev, sum_expect);
    }

    state_mng_abort();
    state_mng_purge_registered_actions_all();
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);
    
    // print for dbg
    state_mng_print_evt_log();
    //test_print_report(1);
    PRINT_LOG_BUFF();

    // get log
    int retval_log = state_mng_get_switch_events(log_buf, sizeof(log_buf)/sizeof(log_buf[0]));
    state_purge_switch_events();
    
    if(0 != retval_log){
        test_assert(0); // to small buf
        printkv(0, "WARNING: state manager test 2, too small buffer \n");
    }  



    // extract only runtime
    u32_t timing_cyc[num_runs];
    int i=0;
    int j=0;  // index of clean, filtered out array
    while(i < sizeof(log_buf)/sizeof(log_buf[0])){
        //printkv(4, "DEBUG: log_buf[%i], from %i, dt %i @ t %i \n", i, log_buf[i].from_state, log_buf[i].t_delta_cyc, log_buf[i].t_cyc);
        if(i < num_runs)
            timing_cyc[i] = -1; // init with error value
        // filter out events not from state END->IDLE
        if(log_buf[i].from_state == CYCLE_STATE_END && log_buf[i].to_state == CYCLE_STATE_IDLE){
            timing_cyc[j] = log_buf[i].t_delta_cyc; // time since reset (to IDLE)
            j++;
        }
        i++;
    }
    test_assert(j == num_runs);
    print_analyze_timing(timing_cyc, num_runs, 2);
    
    int num_err = test_get_err_count();
    printkv(1, "State manager (SM autoconf) test 2... ");  
    printkv(1, (num_err == save_err ? "ok\n" : "failed with %i errors\n"), num_err);

    test_reset();
}


/**
 * @brief: Test throughput of irq1 reset interrupts.
 * Successively lower period betweeen irq1 interrupts.
 * 
 * - Warning: setting irq period is inaccurate
 */
K_THREAD_STACK_DEFINE(thread_sm1_stack, 3000);
struct k_thread thread_sm1_data;
static int thread_sm1_prio = -2;    // cooperative thread
void run_test_sm1_throughput_1(struct device * dev){
    // when choosing numbers, mind integer divison
    int start_period_1_us = 1500; //8000
    int NUM_TS = 7;
    int RUN_T_MS = 100;

    int cur_t_us = start_period_1_us;
    int dt_us = 200;

    printk_framed(0, "Now running sm1 throughput test 1");
    print_dash_line(0);

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);


    for(int i=0; i<NUM_TS; i++){
        // spins until state_mng_start() invoked
        k_tid_t my_tid = k_thread_create(&thread_sm1_data, thread_sm1_stack,
                            K_THREAD_STACK_SIZEOF(thread_sm1_stack), (void (*)(void *, void *, void *))state_mng_run,
                            NULL, NULL, NULL,
                            thread_sm1_prio, 0, K_NO_WAIT);
        printk("state_manager thread @ %p started\n", my_tid);
        sm1_run(dev, cur_t_us, 0);

        printk("DEBUG: test_sm1_throughput_1 going to sleep... \n");
        k_sleep(RUN_T_MS);
        
        printk("DEBUG: Woke up again. Trying to stop SM1. \n");
        // shut down and clean thread
        state_mng_abort();
        state_mng_purge_registered_actions_all();
        // stop firing
        struct DrvValue_uint reg_num = {.payload=0};
    	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);


        state_mng_print_evt_log();
        sm1_print_report();
        sm1_reset();
 
        cur_t_us -= dt_us;

    }

    print_dash_line(0);
}

/**
 * @brief: Test throughput of irq2 (val update) interrupts.
 * Successively lower period betweeen irq2 interrupts.
 * 
 * - irq1 irq period is fixed
 * - Warning: setting irq period is inaccurate
 */
void run_test_sm_throughput_2(struct device * dev, int id_sm){

    int period_irq1_us = 1000;
    // when choosing numbers, mind integer divison

    //int start_period_2_us = 5000;
    int start_divisor = 10; //2
    int delta_divisor = 10;
    //int dt_us = 50; // irq1/2 division must stay integer!
    int NUM_TS = 20;
    int RUN_T_MS = 1000;

    int cur_t_us = period_irq1_us / start_divisor;
   



    printk_framed(0, "Now running sm%i throughput test 2", id_sm);
    print_dash_line(0);

    struct DrvValue_uint val_status_1;
    struct DrvValue_uint val_status_2;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_1);
    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_2);
	printk("Status_1/2 before first run: %u, %u Test may take some seconds... \n", val_status_1.payload, val_status_2.payload);

    int cur_divisor = start_divisor;
    for(int i=0; i<NUM_TS; i++){
       
        // spins until state_mng_start() invoked
        k_tid_t my_tid = k_thread_create(&thread_sm1_data, thread_sm1_stack,
                            K_THREAD_STACK_SIZEOF(thread_sm1_stack), (void (*)(void *, void *, void *))state_mng_run,
                            NULL, NULL, NULL,
                            thread_sm1_prio, 0, K_NO_WAIT);
        printk("state_manager thread @ %p started\n", my_tid);

        // avoid non integer fractions between irq1/2
        cur_t_us = period_irq1_us / cur_divisor;
        switch(id_sm){
            case 1:
                 sm1_run(dev, cur_t_us * cur_divisor, cur_t_us);
                 break;
            case 2:
                 sm2_run(dev, cur_t_us * cur_divisor, cur_t_us, 1, NULL);
                 break;
            default:
                printk("Error: Unknown sm id: %i", id_sm);
                return;
        }
        printk("DEBUG: test_sm1_throughput_1 going to sleep... \n");
        k_sleep(RUN_T_MS);
        
        printk("DEBUG: Woke up again. Trying to stop SM1. \n");
        // shut down and clean thread
        // to stop counting, register clear only cb
        irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_1);
        state_mng_abort();
        k_yield(); // if cooperative main_thread, give sm control to quit thread
        state_mng_purge_registered_actions_all();
        // stop firing
        struct DrvValue_uint reg_num = {.payload=0};
    	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);

        state_mng_print_evt_log();
        PRINT_LOG_BUFF();
        sm1_print_report();
        sm1_reset();
 
        cur_divisor += delta_divisor;

    }
    
    print_dash_line(0);
}

/**
 * @brief:
 * 
 */
// test differnt actions
// supply a single parameter to be varied for each eaction
void run_test_sm2_action_perf_3(struct device * dev){

    #include "state_machines/sm2_tasks.h"

    int period_irq1_us = 1000;
    // when choosing numbers, mind integer divison

    int t_irq_divisor = 10; //2
    int param_start = 4;
    int param_delta = 4;

    int num_runs = 8;
    int t_per_run_ms = 1000;

    int cur_t_us = period_irq1_us / t_irq_divisor;
   
    print_dash_line(0);
    printk_framed(0, "Now running sm2 parametrized action performance test 3");
    print_dash_line(0);

    struct DrvValue_uint val_status_1;
    struct DrvValue_uint val_status_2;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_1);
    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_2);
	printk("Status_1/2 before first run: %u, %u Test may take some seconds... \n", val_status_1.payload, val_status_2.payload);

    int param = param_start;
    for(int i=0; i<num_runs; i++){
       
        // spins until state_mng_start() invoked
        k_tid_t my_tid = k_thread_create(&thread_sm1_data, thread_sm1_stack,
                            K_THREAD_STACK_SIZEOF(thread_sm1_stack), (void (*)(void *, void *, void *))state_mng_run,
                            NULL, NULL, NULL,
                            thread_sm1_prio, 0, K_NO_WAIT);

        printk("state_manager thread @ %p started\n", my_tid);

        // for general purpose
        //sm2_config(32, param, sm2_task_calc_cfo_1, 1, 0);

        // for model calibration
        //sm2_config(32, 8, sm2_task_bench_basic_ops, param, 0);    // mac
        //sm2_config(32, 8, sm2_task_bench_basic_ops, param, 1);    // read
        //sm2_config(32, 8, sm2_task_bench_basic_ops, param, 2);    // write
        //sm2_config(32, 8, sm2_task_bench_basic_ops, param, 3);    // jump
        //sm2_config(param, param, sm2_task_calc_cfo_1, 1, 0);      // t_ov_act
        sm2_config(param, param, sm2_task_calc_cfo_1, 1, 0);        // calib N=f
        //sm2_config(param, 4, sm2_task_calc_cfo_1, 1, 0);          // calib f=4
  
        
        state_mng_purge_filter_state();
        state_mng_add_filter_state(CYCLE_STATE_UL);
        sm2_init(dev, cur_t_us * t_irq_divisor, cur_t_us);
        sm2_run();
        sm2_fire_irqs(cur_t_us * t_irq_divisor, cur_t_us);

        printkv(3, "DEBUG: test_runner going to sleep... \n");
        k_sleep(t_per_run_ms);
        printkv(3, "DEBUG: Woke up again. Trying to stop SM2. \n");
        // shut down and clean thread
        // to stop counting, register clear only cb
        irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_1);
        state_mng_abort();
        k_sleep(1); // if cooperative main_thread, give sm control to quit thread
        state_mng_purge_registered_actions_all();
        // stop firing
        struct DrvValue_uint reg_num = {.payload=0};
    	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);

        

        state_mng_print_evt_log();
        PRINT_LOG_BUFF();
        sm2_print_report();
        sm2_reset();
 
        param += param_delta;

    }
    
    print_dash_line(0);
}

// test differnt actions
// for profiling
void run_test_sm2_action_prof_4(struct device * dev){

    #include "state_machines/sm2_tasks.h"

    int period_irq1_us = 1000;
    // when choosing numbers, mind integer divison

    int t_irq_divisor = 10; //2
    int param_start = 2;
    int param_delta = 2;

    int num_runs = 20;
    int t_per_run_ms = 1000;

    int cur_t_us = period_irq1_us / t_irq_divisor;
   

    printk_framed(0, "Now running sm2 profiler test 4");
    print_dash_line(0);

    struct DrvValue_uint val_status_1;
    struct DrvValue_uint val_status_2;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_1);
    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &val_status_2);
	printk("Status_1/2 before first run: %u, %u Test may take some seconds... \n", val_status_1.payload, val_status_2.payload);

    int param = param_start;
    for(int i=0; i<num_runs; i++){
       
        // spins until state_mng_start() invoked
        k_tid_t my_tid = k_thread_create(&thread_sm1_data, thread_sm1_stack,
                            K_THREAD_STACK_SIZEOF(thread_sm1_stack), (void (*)(void *, void *, void *))state_mng_run,
                            NULL, NULL, NULL,
                            thread_sm1_prio, 0, K_NO_WAIT);

        printk("state_manager thread @ %p started\n", my_tid);

        // to profile sm loop
        sm2_init(dev, cur_t_us * t_irq_divisor, cur_t_us);
        sm2_config(32, 2, sm2_task_calc_cfo_1, 1, 0);
        // disable irq1 and irq2
        sm2_run(); // irq2 used by profiler
        sm2_fire_irqs(0, 0);
       
        // attention:
        // - sm2 should be configured to not wait for irq1 (else idling after state end)
        // means also, that thread will never return 
        // workaround: main thread highest prio, yield task in end state (NOT WORKING)
        // - should be STATE_MNG_LOG_EVENTS_DEPTH = 0

        printk("DEBUG: test_runner going to sleep... \n");
        // test once withouth profiler whether sm is actually running as expected
        //if(i!= 0){
            profiler_enable(10000);  // (1000 cyc)**(-1) = 65 kHz
            profiler_start();
        //}
        k_sleep(t_per_run_ms);

        profiler_stop();
        printk("DEBUG: Woke up again. Trying to stop SM2. \n");
        // shut down and clean thread
        // to stop counting, register clear only cb
        irqtester_fe310_register_callback(dev, IRQ_2, _irq_2_handler_1);
        state_mng_abort();
        k_sleep(1); // if cooperative main_thread, give sm control to quit thread
        state_mng_purge_registered_actions_all();
        // stop firing
        struct DrvValue_uint reg_num = {.payload=0};
    	irqtester_fe310_set_reg(dev, VAL_IRQ_1_NUM_REP, &reg_num);
        irqtester_fe310_set_reg(dev, VAL_IRQ_2_NUM_REP, &reg_num);

        

        state_mng_print_evt_log();
        PRINT_LOG_BUFF();
        sm2_print_report();
        sm2_reset();
 
        param += param_delta;

    }
    
    print_dash_line(0);
}

//#endif  // TEST_MINIMAL