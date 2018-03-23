BOARD ?= arty_fe310
CONF_FILE = prj.conf

# add headers to inculde dir
CFLAGS += -Isrc/drivers/irqtestperipheral/src
CFLAGS += -Isrc/drivers/common

# optimization to allow branch predicting likeley macros
# (is invoked after standard Makefile -Os and thus active)
# disable for weird errors
CFLAGS += -O3 # -fno-delete-null-pointer-checks

# flags no prevent creation of .eh_frame symbol
# doesn't seem to work on zc706
# CFLAGS += -fno-asynchronous-unwind-tables 
# CFLAGS += -fno-exceptions 
# CFLAGS += -fomit-frame-pointer

# debug flags
#CFLAGS += -fno-inline
#CFLAGS += -fno-inline-functions

include ${ZEPHYR_BASE}/Makefile.test



