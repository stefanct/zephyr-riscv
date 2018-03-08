#include "test_suite.h"
#include "utils.h"

static int error_count = 0;
static int error_stamp = 0;

void test_assert(bool expression){
	if(!expression)
		error_count++;
}

void test_warn_on_new_error(void){
	if(error_count > error_stamp){
		printk("WARNING: Test failed with %i errors. Total now %i \n", error_count - error_stamp, error_count);
		error_stamp = error_count;
	}
}

void test_print_report(int verbosity){
    print_report(verbosity, error_count);
}

int test_get_err_count(){
    return error_count;
}

int test_get_err_stamp(){
    return error_stamp;
}

void test_set_err_count(int num_err){
    error_count = num_err;
}

void test_reset(){
    error_count = 0;
    error_stamp = 0;
}