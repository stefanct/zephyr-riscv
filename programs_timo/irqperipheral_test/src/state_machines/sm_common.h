#ifndef SM_COMMON_H
#define SM_COMMON_H

#include <device.h>
#include "states.h"

// actions
void sm_com_clear_valflags();
void sm_com_update_counter();
void sm_com_check_val_updates();
void sm_com_check_last_state();
void sm_com_check_clear_status();

// handlers
void sm_com_handle_timing_goal_start(struct State * state, int t_left);
void sm_com_handle_timing_goal_end(struct State * state, int t_left);
bool sm_com_handle_fail_rval_ul(struct State * state_cur);

// getters
u32_t sm_com_get_i_run();

// setters
void sm_com_set_val_uptd_per_cycle(int val);

// others
void sm_com_reset();
void sm_com_print_report();

#endif