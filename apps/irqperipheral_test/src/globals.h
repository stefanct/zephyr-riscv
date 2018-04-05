/**
 * @file
 * @brief Global shared variables, mainly devices handles.
 * 
 * By convention, write from main.c only.
 */

#include <device.h>

extern struct device * g_dev_irqt;
extern struct device * g_dev_uart0;
extern struct device * g_dev_uart1;