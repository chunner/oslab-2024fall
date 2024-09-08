#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la a0, __bss_start        # load .bss 's start and end addr
    la a1, __BSS_END__

clear_bss:
    bge a0, a1, stack_set;
    sd zero, 0(a0)
    addi a0, a0, 8            # a0 move forward 8 byte
    j clear_bss               # clear next 64 bit mem
stack_set:              # set stack
    li sp, KERNEL_STACK
  

    /* TODO: [p1-task3] enter main function */
    jal main      # jump to main

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    ret
    
    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
