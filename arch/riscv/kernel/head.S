/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define tasknum_loc 0x502001fe
#define taskinfo_offset_loc 0x502001f8
#define taskinfo_size 22
.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* set stack pointer*/ 
stack_set:              
  li sp, KERNEL_STACK
  /*keep the task info*/
  li t0, tasknum_loc      # load task num
  lh a0, 0(t0) 
  li t1, taskinfo_size  
  mul t0, t0, t1        # t0 = task_num * task_size

  # get taskinfo addr
  li t1, taskinfo_offset_loc
  lw t2, 0(t1)  # t2 = the addr of taskinfo

  sub sp, sp, t0
  mv t3, sp

store_taskinfo:
  lbu t4, 0(t2)     # load byte from mem
  sb t4, 0(t3)      # store byre to sp
  addi t2, t2, 1    # move mem pointer
  addi t3, t3, 1    # move stack point
  addi t0, t0, -1   # size --
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

loop:
  wfi
  j loop

END(_start)
