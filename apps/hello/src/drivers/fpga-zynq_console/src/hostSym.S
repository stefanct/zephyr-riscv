#include <kernel_structs.h>

// original version in toolchain buggy
#define SECTION_VAR_2(sect, sym)  .section .sect.sym; sym :

// exports
GDATA(tohost)
GDATA(fromhost)

// each two dwords to be save on rv32
SECTION_VAR_2(tohost, tohost)
    .align 6
    .dword 0
    .dword 0

SECTION_VAR_2(tohost, fromhost)
    .align 6
    .dword 0
    .dword 0

