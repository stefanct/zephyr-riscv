#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>
#include <string.h>

// copy of disabled function in devices.c
extern struct device __device_init_end[];
extern struct device __device_init_start[];
void device_list_get(struct device **device_list, int *device_count)
{

	*device_list = __device_init_start;
	*device_count = __device_init_end - __device_init_start;
}

void print_device_drivers(){
	struct device *dev_list_buf;
	int len;

	device_list_get(&dev_list_buf, &len);

	printk("Found %i device drivers: \n", len);
	for(int i=0; i<len; i++){
		char * name = dev_list_buf[i].config->name;
		printk("%s @ %p, ", (strcmp("", name) == 0 ? "<unnamed>" : name), dev_list_buf[i].config->config_info);
	}
	printk("\n");
}

int main(void){	

    // Attention: what happens if usual serial driver registered as console driver?
	printk("Hello world! \n");
	print_device_drivers();
	printk("This are two very long lines...");
	printk(" without newline in between. Check whether they come out as expected!\n");
	return 2;
}