/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define MASTER_KERNEL_STACK		0xffffffc050502000
#define SLAVE_KERNEL_STACK    0xffffffc050504000
#define tasknum_loc 0xffffffc0502001f6
#define taskinfo_offset_loc 0xffffffc0502001f8 # store the offset of task_info in kernel
#define taskinfo_size 64
#define kernel 0xffffffc050202000
# task_info_new_loc 0x58000010   # user's sp + 0x10
.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bnez t0, slave_kernel // branch if slave mhart

  /* set stack pointer*/ 
stack_set:              
  li sp, MASTER_KERNEL_STACK

  /*move the task info*/
  li t0, tasknum_loc      
  lh a0, 0(t0)          # load task num
  li t1, taskinfo_size  
  mul t0, a0, t1        # t0 = task_num * task_size

  # get taskinfo addr
  li t1, taskinfo_offset_loc
  lw t2, 0(t1)  # t2 = the offset of taskinfo in kernel
  li t1, kernel
  add t2, t2, t1   # t2 = the addr of task_info

  sub sp, sp, t0
  mv t3, sp

store_taskinfo:
  lw t4, 0(t2)     # load word from mem
  sw t4, 0(t3)      # store word to sp
  addi t2, t2, 4    # move mem pointer
  addi t3, t3, 4    # move stack point
  addi t0, t0, -4   # size -=4
  bnez t0, store_taskinfo


  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la a0, __bss_start        # load .bss 's start and end addr
  la a1, __BSS_END__

clear_bss:
  bge a0, a1, jump2main;
  sd zero, 0(a0)
  addi a0, a0, 8            # a0 move forward 8 byte
  j clear_bss               # clear next 64 bit mem

  /* TODO: [p1-task2] setup C environment */
jump2main:
  j main      # jump to main

slave_kernel:
  li sp, SLAVE_KERNEL_STACK
  j main

loop:
  wfi
  j loop

END(_start)
