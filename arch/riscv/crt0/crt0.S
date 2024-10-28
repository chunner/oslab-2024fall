#include <asm.h>
.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la t0, __bss_start        # load .bss 's start and end addr
    la t1, __BSS_END__

clear_bss:
    bge t0, t1, jump2main;
    sd zero, 0(t0)
    addi t0, t0, 8            # a0 move forward 8 byte
    j clear_bss               # clear next 64 bit mem
# stack_set:              # set stack
#     li sp, USER_STACK
  

    /* TODO: [p1-task3] enter main function */
jump2main:
    addi sp,sp, -8
    sd ra, 0(sp)
    jal main      # jump to main
    ld ra, 0(sp)
    addi sp, sp , 8

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    li a7, 1         # SYSCALL_EXIT
    ecall      # call sys_exit
    
    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
