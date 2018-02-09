/**
 * @file
 * @brief Keep track of test errors
 */

#ifndef TEST_SUITE_H
#define TEST_SUITE_H

#include <zephyr.h>


void test_assert(bool expression);
void test_warn_on_new_error(void);
void test_print_report(int verbosity);
int test_get_err_count();
int test_get_err_stamp();
void test_set_err_count(int num_err);
void test_reset();

#endif