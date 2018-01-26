#ifndef SM2_TASKS
#define SM2_TASKS

// actions
void sm2_task_calc_cfo_1();
void sm2_task_bench_basic_ops();

// task config
void sm2_config_user_batch(int num_users);
void sm2_config_bench(int num_macs, int num_loads, int num_writes);

#endif