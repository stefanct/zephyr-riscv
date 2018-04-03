#include <kernel.h>
#include <arch/cpu.h>
#include <uart.h>
#include <board.h>
#include "com_fesvr/syscalls.h"

#define DEV_CFG(dev)						\
	((const struct uart_fpgazynq_device_config * const)	\
	 (dev)->config->config_info)
#define DEV_DATA(dev)						\
	((struct uart_fpgazynq_data * const)(dev)->driver_data)


// todo: check whether dummy values are strcitly necesarry
struct uart_fpgazynq_device_config {
	int dummy; 
};
struct uart_fpgazynq_data {
	int dummy;
};


static unsigned char uart_fpgazynq_poll_out(struct device *dev,
					 unsigned char ch)
{
	printfsvr("%c", ch);
	return ch;	
}

static int uart_fpgazynq_poll_in(struct device *dev, unsigned char *c)
{	
	// no poll in support right now
	return 0;
}

static int uart_fpgazynq_init(struct device *dev)
{
	//const struct uart_fpgazynq_device_config * const cfg = DEV_CFG(dev);

	return 0;
}

static const struct uart_driver_api uart_fpgazynq_driver_api = {
	.poll_in          = uart_fpgazynq_poll_in,   // only supports output
	.poll_out         = uart_fpgazynq_poll_out,
	.err_check        = NULL,
};




static const struct uart_fpgazynq_device_config uart_fpgazynq_dev_cfg_0 = {
	.dummy = 0
};

static struct uart_fpgazynq_data uart_fpgazynq_data_0 = {
	.dummy = 0
};



/**
 * This driver requires
 * - tohost/fromhost symbols which must be set in hostSym.S and the linking script!
 * - set in kconfig menugconfig: 
 * 		- application drivers / fpga-zynq serial driver
 * 		- device drivers / console / "use UART for console" 
 * 		- device drivers / console / "device name" = CONFIG_UART_FPGAZYNQ_NAME
 * - this driver automatically sets drivers / serial. Ignore settings in menuconfig there.
 *   selecting "serial" causes a warning (STM32F4X...), ignore 
 * You can check whether correct hook is installer in uart_console_init()
 */

#if CONFIG_FPGAZYNQ_UART_DRIVER

DEVICE_AND_API_INIT(uart_fpgazynq_0, CONFIG_UART_FPGAZYNQ_NAME,
		    uart_fpgazynq_init,
		    &uart_fpgazynq_data_0, &uart_fpgazynq_dev_cfg_0,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    (void *)&uart_fpgazynq_driver_api);

#endif