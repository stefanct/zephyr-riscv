#include "test_runners.h"
#include "tests/tests.h"
#include "utils.h"
#include "irqtestperipheral.h"
#include "state_manager.h"
#include "log_perf.h"
#include "state_machines/sm1.h"
#include "state_machines/sm2_tasks.h"
#include "tests/test_suite.h"


int run_test_hw_basic_1(struct device * dev, int verbosity){
    //print_banner();
    if(verbosity > 1){
        print_time_banner();
        printk_framed("Now running basic hw test");
        print_dash_line();

        printk_framed("Testing hw rev 1 functionality");  
        print_dash_line();
    }
    test_hw_rev_1_basic_1(dev, verbosity);
    if(verbosity > 1){
        test_print_report();  
        print_dash_line();
        printk_framed("Testing hw rev 2 functionality");
        
        print_dash_line();
    }
    test_hw_rev_2_basic_1(dev, verbosity);
    if(verbosity > 1){
        test_print_report();
        print_dash_line();
        printk_framed("Testing hw rev 3 functionality");
        print_dash_line();
    }
    test_hw_rev_3_basic_1(dev, verbosity);

    if(verbosity > 1){
        test_print_report();
        print_dash_line();
        test_print_report(); // global var from tests.c
    }
    
    int num_err = test_get_err_count();
    test_reset();
    if(verbosity > 0){
        // manually change for new hw rev
        printk("Hardware test for rev %i... ", 3);    
        if(num_err == 0)
            printk("ok \n");
        else
            printk("FAILED with %i errors\n", num_err);
    }
    return num_err;
}


void run_test_timing_rx(struct device * dev){    
    #ifndef TEST_MINIMAL 
    // currently, fifo functionality broken

    int NUM_RUNS = 10;		// warning high values may overflow stack
	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    test_reset();

    print_banner();
    print_time_banner();
    

    printk_framed("Now running raw irq to isr timing test");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, &irq_handler_mes_time);
    test_interrupt_timing(dev, timing_detailed_cyc, NUM_RUNS, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    //return;

    printk_framed("Now running timing test with queues and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0,_irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    printk_framed("Now running timing test with queues and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    printk_framed("Now running timing test with fifos and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with fifos and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with semArr and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with semArr and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    printk_framed("Now running timing test with valflags and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_1);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with valflags and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with hand-optimized valflag plus queue");
    print_dash_line();
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_5);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 4, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    // tests don't load values, so don't count errors here
    // -> reset error counter
    
    int errors_sofar = test_get_err_stamp();
    print_dash_line();
    printk("INFO: So far %i errors. Ignore the following error warnings for tests without load \n", errors_sofar);
    printk_framed("Now running timing test with noload, queue and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, fifos and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, semArr and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, valflags and direct reg getters");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with minimal, valflags\n");
    irqtester_fe310_register_callback(dev, 0, _irq_0_handler_4);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    // reset counter
    test_set_err_count(errors_sofar);
    printk("INFO: End of noload. Resetting error count to %i \n", test_get_err_count());
    print_dash_line();

    // restore default handler
    irqtester_fe310_register_callback(dev, IRQ_0, _irq_gen_handler);
    irqtester_fe310_register_callback(dev, IRQ_1, _irq_gen_handler);
    irqtester_fe310_register_callback(dev, IRQ_2, _irq_gen_handler);

    test_print_report(); // global var from tests.c
    #endif // TEST_MINIMAL
}


void run_test_min_timing_rx(struct device * dev){    
    
    int NUM_RUNS = 100;		// warning high values may overflow stack
    int mode = 3;

	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    //int * timing_detailed_cyc = NULL;
    test_reset();

    switch(mode){
        case 4:
            printk_framed("Now running timing test with hand-optimized valflag plus queue");
             irqtester_fe310_register_callback(dev, 0, _irq_0_handler_5);
            break;
        case 3:
            printk_framed("Now running timing test with hand-optimized valflag only");
             irqtester_fe310_register_callback(dev, 0, _irq_0_handler_6);
            break;
        default:
            printk("ERROR: Not implemented \n");
            test_assert(0);
            return;
    }
 
   
    print_dash_line();
    // note: defining TEST_MINIMAL in driver sets callback in init function
    // might help compiler to optimize
   
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, mode, verbosity);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    //
    //printk_framed("Now running timing test with queues and direct reg getters");
    //print_dash_line();
    //irqtester_fe310_register_callback(dev, 0, _irq_gen_handler);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    //print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    //print_dash_line();

    test_print_report(); // global var from tests.c

}

void run_test_irq_throughput_1(struct device * dev){

    int num_runs = 10;

    // when choosing numbers, mind integer divison
    int START_DT_CYC = 20000;
    int NUM_TS = 200;
    int cur_t_cyc = START_DT_CYC;
    int dt_cyc = START_DT_CYC / NUM_TS;

    int status_res[NUM_TS];

    printk_framed("Now running interrupt throughput test 1");
    print_dash_line();

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);
    int status_stamp = 0;

    for(int i=0; i<NUM_TS; i++){
        int status = 0;
        test_irq_throughput_1(dev, cur_t_cyc, &status, num_runs);
        printk("From %i runs: dt_cyc / status / status_tot: {[%i, %i, %i ]}\n", num_runs, cur_t_cyc, status - status_stamp, status);
        status_res[i] = status - status_stamp;
        status_stamp = status;
        cur_t_cyc -= dt_cyc;

    }

    // printing results
    printk("From %i runs: dt_cyc\n", num_runs);
    cur_t_cyc = START_DT_CYC;
    printk("{[");
    for(int i=0; i<NUM_TS; i++){
        printk("%i, ", cur_t_cyc);
        cur_t_cyc -= dt_cyc;
    }
    printk("]} \n");

    printk("From %i runs: status\n", num_runs);
    printk("{[");
    print_arr_int(status_res, NUM_TS);
    printk("]}\n");
    
    print_dash_line();

}


void run_test_irq_throughput_2(struct device * dev){

    int num_runs = 100; // should equal to tests::STATUS_ARR_LEN

    // when choosing numbers, mind integer divison
    int start_period1_us = 250;
    int num_ts = 50;
    int cur_t_us = start_period1_us;
    int dt_us = 5;

      
    printk_framed("Now running interrupt throughput test 2");
    print_dash_line();

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);

    int status_arr_len = 100;
    int status_arr[status_arr_len];

    for(int i=0; i<num_ts; i++){

        test_irq_throughput_2(dev, 65*cur_t_us, num_runs, status_arr, status_arr_len);
        printk("From %i runs with irq1 period %u us, %u cpu cycles. \n", num_runs, cur_t_us, 65*cur_t_us);

        printk("Detailed missed clear status per run \n");
	    print_arr_int(status_arr, status_arr_len);

        cur_t_us -= dt_us;

    }

    irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
	printk("Status_1 after run: %i.  \n", status_1.payload);

    print_dash_line();
    test_print_report();
}


int run_test_irq_throughput_3_autoadj(struct device * dev, int verbosity){
    
    int num_runs = 20; // runs per t
    u32_t guess_t_cyc = 1000;
    u32_t delta_cyc = 50;   // num_ts * delta_cyc should be enough to get close to 0
    int num_ts = 200;

    struct DrvValue_uint status_1;
	irqtester_fe310_get_reg(dev, VAL_IRQ_1_STATUS, &status_1);
    
    if(verbosity == 1){
        printk("Running irq1 throughput test...");
    }
    else if(verbosity > 1){
        print_dash_line();
        printk_framed("Now running interrupt throughput test 3 (auto adjust) for irq1 ");
        print_dash_line();
        printk("Status_1 before first run: %i. Test may take some seconds... \n", status_1.payload);
    }
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
            if(verbosity > 2){
                bool overwrite_line = false;
                // overwrite same last result for verbosity 2 only
                if(verbosity == 2){
                    if(cur_t_cyc == succes_t_cyc)
                        overwrite_line = true;
                    if(overwrite_line)
                        printk("\r");   // overwrite console
                    else
                        printk("\n");
                }
                printk("[%i] Cleared interrupt with period t= %u cyc, t= %u us => f= %u kHz", i, cur_t_cyc, cur_t_cyc/65, 65000 / (cur_t_cyc)); 
                if(verbosity > 3){
                    printk("\n");
                    printk("Status array: \n");
                    print_arr_int(status_arr, status_arr_len);
                }
            }

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
    
    if(verbosity == 1){
         printk(" t= %i cyc, f= %i kHz, status_1: %i.\n", 
            succes_t_cyc, 65000 / succes_t_cyc, status_1.payload);
    }
    else if(verbosity > 1){
        printk("\nFinished with irq t= %i cyc, f= %i kHz. Status_1 after run: %i.  \n",
             succes_t_cyc, 65000 / succes_t_cyc, status_1.payload);
        printk("Last cache warmup status_1 behaviour: \n");
        print_arr_int(status_arr_success, status_arr_len);
        print_dash_line();
    }
    

    return succes_t_cyc;
    
}

void run_test_poll_throughput_1_autoadj(struct device * dev){
    
    int num_runs = 20; // runs per t
    u32_t guess_t_cyc = 5000;
    u32_t delta_cyc = 50;   // num_ts * delta_cyc should be enough to get close to 0
    int num_ts = 250;

    printk_framed("Now running poll throughput test 1 with auto adjust");
    print_dash_line();

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
                printk("\r");   // overwrite console
            else
                printk("\n");
            printk("[%i] Cleared poll dev with period t= %u cyc, t= %u us => f= %u kHz", i, cur_t_cyc, cur_t_cyc/65, 65000 / (cur_t_cyc)); 
            
            printk("\nCache warmup status_3 behaviour: \n");
            print_arr_int(status_arr, status_arr_len);

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
	printk("\nFinished, status_3 after run: %i.  \n", status_3.payload);
    printk("Last cache warmup status_1 behaviour: \n");
    print_arr_int(status_arr_success, status_arr_len);

    print_dash_line();
}

// set value register to sum of states since START
// so while testing, after every fire() when returning back to IDLE
// state_sum is the sum of state_ids collected when running through the cycle
// we cann assert on this
static struct device * dev_testscope;
void action_test_mng_1(void){
    static int state_sum;
    cycle_state_id_t state = state_mng_get_current();

    if(state == CYCLE_STATE_START){
        state_sum = 0;
    }
    state_sum += state;

    printk("Test action fired in state %i, state_sum: %i \n", state, state_sum);

    struct DrvValue_uint setval = {.payload=state_sum};

    irqtester_fe310_set_reg(dev_testscope, VAL_IRQ_0_VALUE, &setval);

}

K_THREAD_STACK_DEFINE(thread_mng_run_1_stack, 2048);
struct k_thread thread_mng_run_1_data;
void run_test_state_mng_1(struct device * dev){
    // todo: this testcase is broken, for preempt sm therad only
    // fix and include substate logic in test
    int NUM_RUNS = 10;	
    dev_testscope = dev;

    print_dash_line();
    printk_framed("Now running state manager test 1");
    print_dash_line();

    // auto config, todo: make custom config for this test
    // in order to reduce coupling to states::auto implementation
    state_mng_configure(NULL, NULL, 0, 0);

    printk("ATTENTION: Requesting value %i, expect load warnings in STATE_START. Something is wrong if none! \n", VAL_IRQ_0_VALUE);
    irqt_val_id_t reqvals[] = {VAL_IRQ_0_VALUE};   // input, this flag will never be set!
    state_mng_register_action(CYCLE_STATE_IDLE, action_test_mng_1, NULL, 0);
    state_mng_register_action(CYCLE_STATE_START, action_test_mng_1, reqvals, 1);
    state_mng_register_action(CYCLE_STATE_END, action_test_mng_1, NULL, 0);

    state_mng_init(dev);

     // state config done, print
    state_mng_print_state_config();
    state_mng_print_transition_table_config();

	// start the thread immediatly
	k_tid_t my_tid = k_thread_create(&thread_mng_run_1_data, thread_mng_run_1_stack,
                                 K_THREAD_STACK_SIZEOF(thread_mng_run_1_stack), (void (*)(void *, void *, void *)) state_mng_run,
                                 NULL, NULL, NULL,
                                -1, 0, K_NO_WAIT);
    printk("state_manager thread @ %p started\n", my_tid);

    test_assert(state_mng_start() == 0);

    for(int i=0; i<NUM_RUNS; i++){
        // fire (and run a state achine cycle) only every 100ms 
        k_sleep(100); //ms
        test_state_mng_1(dev);
    }

    state_mng_abort();
    state_mng_purge_registered_actions_all();

    test_print_report();
    
}


/**
 * Test throughput of irq1 reset interrupts.
 * Successively lower period betweeen irq1 interrupts.
 * 
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

    printk_framed("Now running sm1 throughput test 1");
    print_dash_line();

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

    print_dash_line();
}

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
   



    printk_framed("Now running sm%i throughput test 2", id_sm);
    print_dash_line();

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
    
    print_dash_line();
}

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
   

    printk_framed("Now running sm2 parametrized action performance test 3");
    print_dash_line();

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

        //sm2_config(32, param, sm2_task_calc_cfo_1, 1, 0);

        // for model calibration
        //sm2_config(32, param, NULL, 1, 0);
        //sm2_config(param, 4, sm2_task_calc_cfo_1, 1, 0);
        sm2_config(param, param, sm2_task_calc_cfo_1, 1, 0);
        //sm2_config(32, 8, sm2_task_bench_basic_ops, param, 2);
       
        sm2_run(dev, cur_t_us * t_irq_divisor, cur_t_us, 1, 0);
        printk("DEBUG: test_runner going to sleep... \n");
        k_sleep(t_per_run_ms);
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
    
    print_dash_line();
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
   

    printk_framed("Now running sm2 profiler test 4");
    print_dash_line();

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
        sm2_config(32, 2, sm2_task_calc_cfo_1, 1, 0);
        // disable irq1 and irq2
        sm2_run(dev, 0, 0, 1, 0); // irq2 used by profiler
       
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
    
    print_dash_line();
}