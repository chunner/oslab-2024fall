#include <asm.h>
.section ".entry_function","ax"
ENTRY(_start)
    addi sp, sp, -16
    sd a0, 0(sp)
    sd a7, 8(sp)
    // sys call to record brk
    li a7, 61         # SYSCALL_GET_BRK
    la a0, __BSS_END__
    ecall   
    ld a0, 0(sp)
    ld a7, 0(sp)
    addi sp, sp, 16

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
    
    /************************************************************/
    /* TODO: [P3] call sys_exit */
    li a7, 1         # SYSCALL_EXIT
    ecall      # call sys_exit
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
