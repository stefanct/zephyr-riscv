Zephyr on fpga-zynq
=========================

This repo is a fork of the rv32 port of Zephyr (v 1.8.99) and allows to run on a fpga-zynq platform. Especially, it comes with a modified linking script and a fake serial driver to enable communication with the fpga-zynq frontend server (fesvr). 

There are two applications bundled with this repo.


1. Look into `apps/hello` to get started.
This example is meant to illustrate how the fake serial driver can be used to output to the fesvr.

2. The `apps/irqperipheral_test` runs a FSM on top of a special hardware block which can be programmed to throw IRQs. It won't run without the irqtestperipheral (a periphery device to rocket chip). The scala sources for the irqt peripheral can be found in [this repo](https://github.com/timoML/fpga-zynq/tree/new-devices-cow).
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

