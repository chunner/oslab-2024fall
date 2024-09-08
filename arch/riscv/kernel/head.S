/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la a0, __bss_start        # load .bss 's start and end addr
  la a1, __BSS_END__

clear_bss:
  bge a0, a1, stack_set;
  sd zero, 0(a0)
  addi a0, a0, 8            # a0 move forward 8 byte
  j clear_bss               # clear next 64 bit mem

  /* TODO: [p1-task2] setup C environment */
stack_set:              # set stack
  li sp, KERNEL_STACK
  
  j main      # jump to main

loop:
  wfi
  j loop

END(_start)
