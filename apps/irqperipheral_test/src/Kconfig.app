menuconfig APP_RISCV_CORE_FREQ_MHZ
	int "Clock speed of the riscv core in MHz"
	default 65
	help 
	Only for convenience.
	Not used for any kernel functionality, which is based on the sysclock.
	
menuconfig APP_LINK_ALL_FLASH
	bool "Set linker to put all sections in flash (ROM)"
	default y
	help
	Reliable, but slow. Should disable XIP when used.

menuconfig APP_LOG_PERF_BUFFER_DEPTH
	int "Size of buffer for LOG_PERF macro. Set 0 for best perf."
	default 10

menuconfig APP_LOG_PERF_STRING_LEN
	int "Length (in chars) for single LOG_PERF message" 
	default 60

menuconfig APP_PROFILER_BUF_DEPTH
	int "Size of buffer for profiling. Set 0 for best perf."
	default 0

menuconfig APP_SM_LOG_DEPTH
	int "Size of log event buffer of state_manager. Set 0 for best perf."
	default 32