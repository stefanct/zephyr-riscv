Zephyr on fpga-zynq
=========================

This repo is a fork of the rv32 port of Zephyr (v 1.8.99) and allows to run on a fpga-zynq platform. Especially, it comes with a modified linking script and a fake serial driver to enable communication with the fpga-zynq frontend server (fesvr). 

There are two applications bundled with this repo.


1. Look into `apps/hello` to get started.
This example is meant to illustrate how the fake serial driver can be used to output to the fesvr.

2. The [`apps/irqperipheral_test`](#irqt) runs on top of a special hardware block which can be programmed to throw IRQs. It won't run without the IRQTestPeripheral (a periphery device to rocket chip). The scala sources for the irqt peripheral can be found in [this repo](https://github.com/timoML/fpga-zynq/tree/new-devices-cow).
Even without, the code may illustrate how to implement drivers that serve IRQs thrown by a custom RISC-V peripheral device.


For further documentation on zephyr, also check:
- Zephyr port to RISC-V [README](https://github.com/fractalclone/zephyr-riscv)
- Official Zephyr RTOS [documentation](http://docs.zephyrproject.org/index.html)


Requirements
------------------


Please make sure that you fulfill the following requirements, before trying to compile & run zephyr apps.

Hardware:
- Xilinx ZC706 development board

   Currently, only this specific board is supported. However, it should be straightforward porting to other zynq platforms supported by fpga-zynq.

- 32 bit (rv32) RISC-V core synthesized on this FPGA

    Look [here](https://github.com/timoML/fpga-zynq/tree/new-devices-cow), to obtain a tested rv32 rocket-core instance or to learn which pitfalls can prevent your own config from working.

Software:
- fesvr-zynq

    Used to load binaries to the rocket core and printing output from the fake serial. Please test, whether you can run the bare metal hello test, as described [here](https://github.com/timoML/fpga-zynq/tree/new-devices-cow#programs)
- toolchain

    For compiling rv32 binaries. Easiest is to use the SDK (0.9.1) that comes with zephyr.


Toolchain set-up
------------------

Zephyr comes with an SDK that includes a riscv32-unknown-elf toolchain.
Instructions to install and set-up the SDK for compilation can be found [here](https://github.com/fractalclone/zephyr-riscv). Also follow this link, to learn how to set environmental variables for compilation in case you'd like to use your own toolchain.


Building apps for fpga-zynq
------------------


A Zephyr application typically links statically aginst the kernel and is compiled into a single .elf binary file. 
To start, first navigate to the zephyr root dir and

    $ source zephyr-env.sh 

Goto the apps dir 

    $ cd apps/hello

Check whether the kernel config is correct, as described [here](#kconfig). If so, you can start compilation by 

    $ make BOARD=zc706 

If using a riscv64-unknown-elf toolchain, mind to add `-mabi=ilp32 -march=rv32imac` flags to your `make` command. Providing additional flags to the gcc compiler can be done as follows

    $ make BOARD=zc706 CFLAGS="-mabi=ilp32 -march=rv32imac"

Now, you can copy the binary `outdir/zc706/zephyr.elf` to the board. We recommend using an ethernet connection ([setup](https://github.com/ucb-bar/fpga-zynq#appendices)) to connect with the FPGA, ssh for terminal sessions and SCP for file transfer. Finally, open a terminal session to the zc706 and

    $ ./fesvr-zynq zephyr.elf

You should see all output from `printk` in the terminal. If you don't see any output, first check that you followed this readme correctly. Further help can be found in the [Troubleshooting](#trouble) section. 



<a name="trouble"></a> Troubleshooting
------------------

If you don't see any output from zephyr although the fesvr fake serial driver is configured correctly, first make sure that you can run [simple bare metal programs](https://github.com/timoML/fpga-zynq/tree/new-devices-cow#programs).

In case these run as expected, you need to debug the boot process using Xilinx chipscope. Setting up the hardware for debugging is described [here](https://github.com/timoML/fpga-zynq/tree/new-devices-cow#trouble).


### Following the Zephyr boot

Given that the rocket core receives the msip interrupt correctly, the boot process involves the following important steps. Follow them by tracing the program counter in the waveform.

The rocket bootrom (not part of this repo) is executed right after the msip from `0x10000`
- `fpga-zynq/common/bootrom/bootrom.S`

After returning to the address stored in the mepc reg (usually `0x80000000`), zephyr should take over. It boots by:
- `__start` in `arch/riscv32/core/reset.S`
- `_PrepC` in `arch/riscv32/core/prep_c.c`
- `_Cstart`  in `kernel/init.c`

and should then switch to the `main()` function of your application.

<a name="kconfig"></a> Configuration of kernel & apps
------------------


The Zephyr kernel is configured using Kconfig files distriuted among the 
folder structure. Some information about it can be found [here](http://docs.zephyrproject.org/1.6.0/reference/kbuild/kbuild.html).
In an application directory, the easisiet way to modify the kernel
configuration is to

    $ make menuconfig BOARD=zc706

### Necessary kernel configuration

For enabling output to fesvr, set in menugconfig: 

- application drivers / fpga-zynq serial driver

    and note the name of the driver. 

Then, set:

- device drivers / console / "use UART for console" 
- device drivers / console / "device name" = noted name of fpga-zynq serial driver

### Board default values

If you modified parameters of the rocket configuration (eg. memory size), you may need to alter some of the board's default kernel configuration values.
They are are mainly set in 
- `boards/riscv32/zc706/zc706_defconfig`
- `arch/riscv32/soc/riscv-privilege/zc706/Kconfig.defconfig.series`

In addition, there are prj.conf files in every application dir to overwrite default values. This mechanism might be helpful, if you have multiple configurations of your rocket chip running on the same board.

After compilation, it is good practice to check in the application's `outdir/zc706/.config` file, whether values were not unexpectetly overwritten by some dependency.

### App configuration

In this repo, also apps can define Kconfig parameters by sourcing their Kconfig files in `Kconfig.zephyr` and `apps/Kconfig`. This allows to set global constants (with prefix `CONFIG_`) that can be 
easily altered by `make menuconfig`. Since drivers (eg. the fake serial driver for fesvr) live currently in the application dir, also their parameters are set in the same way. 


<a name="irqt"></a> IrqTestPeripheral app
-------------------

The `apps/irqperipheral_test` application is meant as an example how to interact with a rocket peripheral device throwing IRQs. 

### Doxygen documentation

There is a (incomplete) doxygen documentation available. You can look at it opening
`apps/irqperipheral_test/doc/html/files.html` in your browser.

To regenerate the documentation, you may use

    $ doxywizard

and load the settings file `apps/irqperipheral_test/doc/Doxyfile` before hitting `Run/Run doxygen`.


### Benchmarks

The following bencharks can be run:

- IRQ benchmarks

  Find in `test_runners.c::run_test_irq_throughput_<1-3>()`
  
  For absolute values on IRQ latency, we **strongly recommend** to chipscope the PC from entering `arch/riscv32/core/isr.S` till the end of the ISR.

  Since software measurments of IRQ latency are inaccurate, some tests benchmarking the 'IRQ througput' as a more exact estimate for IRQ latency are provided. The idea is to program a periodically thrown interrupt which is cleared in the ISR. In theory, lowering the period up to the point where the IRQ cannot be cleared anymore provides the IRQ troughput. Unfortunantely, the results do not line up with what we measured by chipscoping into the FPGA. Nevertheless, values can be compared against each other when playing with the Zephyr interrupt handling code. 

- Driver RX latency benchmarks

  Find in `test_runners.c::run_test_timing_rx()`

  Software measurement of the latency between a fired IRQ and this being registered by a consuming thread. As above, software measurments don't give good absolute values. However, can be used to compare different kernel objects (semaphores, queues, fifos, atomic flags) for up-passing events from the device driver to a thread.

- FSM critical loop

  Find in `test_runners.c::run_test_state_mng_2()`

  Software measurement of the latency between entering CYCLE_STATE_START and leaving CYCLE_STATE_END for the auto config of the state_manager. Use to benchmark the critical FSM loop, especially to see variability in the execution time.


### <a name="fsm"></a> Running a Finite State Machine (FSM)

In the follwing, we describe the incredients used in `state_machines/sm_template.c` to build a simple state machine.

The `state_manager` provides most of the FSM logic and is generally able to react to any kind of event, sent to it via a queue.
Specific implementations are avilable in the `state_machines` folder. They are  driven by IRQs thrown by the IrqTestperipheral. 
Since our FSMs are meant to power wirless hardware, a state in our notion is connected to a timeslot in a protocol cycle. Therefore, the FSM's default behaviour is to switch to a new state (timeslot), if no event has occured.
Look into `test_runners.c::run_test_sm2_action_perf_3()` and `sm2.c` to see a more evolved state machine.

Incredients of `sm_template.c`: 

1. State machine definition 

   - First, define your `State` structs. All available `id_name`s are defined in `states.h`. You need to specify a `default_next_state` to which the FSM will switch next. If the same state should be repeated for a fixed number of times, use the substate logic provided by `states_configure_substates()`.
    
    ```C
    static struct State sm_t_idle 
    = {.id_name = CYCLE_STATE_IDLE, .default_next_state = CYCLE_STATE_IDLE};
    static struct State sm_t_start 
    = {.id_name = CYCLE_STATE_START, .default_next_state = CYCLE_STATE_END};
    static struct State sm_t_end 
    = {.id_name = CYCLE_STATE_END, .default_next_state = CYCLE_STATE_IDLE};

    ```
   - Also, you need arrays to hold all states and the transition table of the state machine.

    ```C
    static struct State sm_t_states[_NUM_CYCLE_STATES];
    static cycle_state_id_t sm_t_tt[_NUM_CYCLE_STATES][_NUM_CYCLE_EVENTS];
    ```

   - Fill your state array and transition table and configure the `state_manger` via `state_mng_configure()` to use it.

    ```C
    // init state array
    sm_t_states[CYCLE_STATE_IDLE] = sm_t_idle;
    sm_t_states[CYCLE_STATE_START] = sm_t_start;
    sm_t_states[CYCLE_STATE_END] = sm_t_end;
    // init transition table
    for(int i=0; i<_NUM_CYCLE_STATES; i++){
        sm_t_tt[i][CYCLE_EVENT_RESET_IRQ] = CYCLE_STATE_START;   
    }
    // pass sm2 config to state manager
    state_mng_configure(sm2_states, (cycle_state_id_t *)sm2_tt, _NUM_CYCLE_STATES, _NUM_CYCLE_EVENTS);

    ```


2. Actions 

    - Register actions via `state_mng_register_action()` that should be executed when a state is invoked.
    ```C
    // action fired in non-idle states
    state_mng_register_action(CYCLE_STATE_START, action_hello, NULL, 0);
    state_mng_register_action(CYCLE_STATE_END, action_hello, NULL, 0);

    ```
 

3. Run and fire methods 

    - Create a thread running the `state_manager` main loop `state_mng_run` and give `state_mng_init()`, `state_mng_start()` to let it run. If your SM should be driven by IRQs from the IrqTestPeripheral, configure it to fire. Be aware that the `state_manager` thread is usually configured as cooperative thread. Therefore,it will return control only if waiting for events in the `CYCLE_STATE_IDLE` state.
    ```C
    k_thread_create(&thread_mng_run_1_data, thread_mng_run_1_stack,
                        K_THREAD_STACK_SIZEOF(thread_mng_run_1_stack), (void (*)(void *, void *, void *)) state_mng_run,
                        NULL, NULL, NULL,
                        thread_mng_run_1_prio, 0, K_NO_WAIT);
    state_mng_init(g_dev_irqt);
    if(0 != state_mng_start()){
        printk("Error while starting state_manager \n");
        return;
    }
    sm_t_fire_irqs(200000); // us
    ```
    - Give control to the state_manager thread
    ```C
    k_sleep(200);  // ms
    
    ```
    - Stop the thread. You should see the printed output of the registered action in the console. In addition, you may access the event log. To this end, mind to enable logging in Kconfig.
    ```C
    state_mng_abort();

    // reset and stop firing
    irqtester_fe310_reset_hw(g_dev_irqt);
    // clean up sm
    state_mng_purge_registered_actions_all();
    // see (via logging) state_manager log
    state_mng_print_evt_log();
    ```

4. Optional: Timing goals (see eg. `sm2.c::config_timing_goals()`)

    - Goals that are checked upon entering and leaving a state. Handlers can be provided via `states_set_handler_timing_goal_start()`/`_end()` and can take action if goals are missed or hit (see `sm2.c::config_handlers()`). 

5. Optional: Requested values (see eg. `sm2.c::config_handlers()`)

   - If an action within in a state requires certain input values from the irqt driver, it can check whether they have been flagged already available. Therefore: 1. Speficy the `DrvValue`  by its `id_name` when registering the action. 2. Set a handler via `states_set_handler_reqval()`. Handlers should return `bool` to indicate whether task actions should be skipped. For example, `sm_common::sm_com_handle_fail_rval_ul()` waits on values until the timing goal of the state is reached and warns, if the value didn't become available.  

