#include "test_runners.h"
#include "tests/tests.h"
#include "utils.h"
#include "irqtestperipheral.h"
#include "state_manager.h"



void run_test_hw_basic_1(struct device * dev){
    print_banner();
    print_time_banner();

    printk_framed("Now running basic hw test");
    print_dash_line();

    test_hw_rev_1_basic_1(dev);

    print_dash_line();
    print_report(error_count); // global var from tests.c
}


void run_test_timing_rx(struct device * dev){    
    #ifndef TEST_MINIMAL 
    // currently, fifo functionality broken

    int NUM_RUNS = 10;		// warning high values may overflow stack
	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    error_count = 0;
    error_stamp = 0;

    print_banner();
    print_time_banner();
    

    printk_framed("Now running raw irq to isr timing test");
    print_dash_line();
    irqtester_fe310_register_callback(dev, &irq_handler_mes_time);
    test_interrupt_timing(dev, timing_detailed_cyc, NUM_RUNS, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with queues and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    printk_framed("Now running timing test with queues and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    printk_framed("Now running timing test with fifos and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with fifos and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with semArr and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with semArr and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    printk_framed("Now running timing test with valflags and direct reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with valflags and generic reg getters");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with hand-optimized valflag plus queue");
    print_dash_line();
    irqtester_fe310_register_callback(dev, _irq_0_handler_5);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 4, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    // tests don't load values, so don't count errors here
    // -> reset error counter
    
    int errors_sofar = error_stamp;
    print_dash_line();
    printk("INFO: So far %i errors. Ignore the following error warnings for tests without load \n", errors_sofar);
    printk_framed("Now running timing test with noload, queue and direct reg getters");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, fifos and direct reg getters");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, semArr and direct reg getters");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk_framed("Now running timing test with noload, valflags and direct reg getters");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk_framed("Now running timing test with minimal, valflags\n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_4);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    // reset counter
    error_count = errors_sofar;
    printk("INFO: End of noload. Resetting error count to %i \n", error_count);
    print_dash_line();

    // restore default handler
    irqtester_fe310_register_callback(dev, _irq_0_handler);

    print_report(error_count); // global var from tests.c
    #endif // TEST_MINIMAL
}


void run_test_min_timing_rx(struct device * dev){    
    
    int NUM_RUNS = 100;		// warning high values may overflow stack
    

	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    //int * timing_detailed_cyc = NULL;
    error_count = 0;
    error_stamp = 0;


    printk_framed("Now running timing test with hand-optimized valflag plus queue");
    print_dash_line();
    // note: defining TEST_MINIMAL in driver sets callback in init function
    // might help compiler to optimize
    irqtester_fe310_register_callback(dev, _irq_0_handler_5);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 4, verbosity);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    //
    //printk_framed("Now running timing test with queues and direct reg getters");
    //print_dash_line();
    //irqtester_fe310_register_callback(dev, _irq_0_handler);
    //test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    //print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    //print_dash_line();

    print_report(error_count); // global var from tests.c

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

    /* old
    irqt_val_id_t reqvals[] = {1,2,3};
    irqt_val_id_t reqvals2[] = {1,2,3,4,5,6};
    state_mng_register_action(CYCLE_STATE_START, action_print_start_state, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END, action_print_state, NULL, 0);

    state_mng_register_action(CYCLE_STATE_START, action_print_start_state, reqvals, 3);
    state_mng_register_action(CYCLE_STATE_END, action_print_state, reqvals2, 6);
    state_mng_register_action(CYCLE_STATE_START, action_print_start_state, reqvals2, 6);
    */

    state_mng_init(dev);

	// start the thread immediatly
	k_tid_t my_tid = k_thread_create(&thread_mng_run_1_data, thread_mng_run_1_stack,
                                 K_THREAD_STACK_SIZEOF(thread_mng_run_1_stack), state_mng_run,
                                 NULL, NULL, NULL,
                                 0, 0, K_NO_WAIT);

    for(int i=0; i<NUM_RUNS; i++){
        // fire (and run a state achine cycle) only every 100ms 
        k_sleep(100); //ms
        test_state_mng_1(dev);
    }

    state_mng_abort();
    state_mng_purge_registered_actions_all();

    print_report(error_count);
    
}