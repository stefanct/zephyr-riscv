#include "tests/tests.h"
#include "utils.h"
#include "irqtestperipheral.h"
#include "test_runners.h"
#include <device.h>



void run_test_timing_rx(struct device * dev){    
    
    #define NUM_RUNS 10		// warning high values may overflow stack
	int verbosity = 1;
	int timing_detailed_cyc[NUM_RUNS];
    error_count = 0;
    error_stamp = 0;

    print_banner();
    print_time_banner();
    
    printk("Now running raw irq to isr timing test \n");
    irqtester_fe310_register_callback(dev, &irq_handler_mes_time);
    test_interrupt_timing(dev, timing_detailed_cyc, NUM_RUNS, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk("Now running timing test with queues and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    
    printk("Now running timing test with queues and generic reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    
    printk("Now running timing test with fifos and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with fifos and generic reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk("Now running timing test with semArr and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with semArr and generic reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    printk("Now running timing test with valflags and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with valflags and generic reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_2);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    
    // tests don't load values, so don't count errors here
    // -> reset error counter
    
    int errors_sofar = error_stamp;
    print_dash_line();
    printk("INFO: So far %i errors. Ignore the following error warnings for tests without load \n", errors_sofar);
    printk("Now running timing test with noload, queue and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 0, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with noload, fifos and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 1, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with noload, semArr and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 2, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    printk("Now running timing test with noload, valflags and direct reg getters \n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_3);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();

    printk("Now running timing test with minimal, valflags\n");
    irqtester_fe310_register_callback(dev, _irq_0_handler_4);
    test_rx_timing(dev, timing_detailed_cyc, NUM_RUNS, 3, verbosity);
    print_analyze_timing(timing_detailed_cyc, NUM_RUNS, verbosity);
    print_dash_line();
    // reset counter
    error_count = errors_sofar;
    printk("INFO: End of noload. Resetting error count to %i \n", error_count);
    print_dash_line();

    print_report(error_count); // global var from tests.c

}