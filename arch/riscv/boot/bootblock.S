#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

// sd card read byte per time
.equ sd_read_size, 0x8000  // 512 * 64

// slave hart enter bootblock, lock; enter kernel, unlock  
.equ slave_hart_lock, 0x502001f4

.text
.global main

main:
// fence on all memory and I/O
	fence
// a0 is mhartid
	bnez a0, secondary

// call BIOS to print string "It's bootblock!"
	li a7, BIOS_PUTSTR
	la a0, msg					// char *str
	jal bios_func_entry

// call BIOS to read kernel in SD card
	// 64 sector per time
	li s0, kernel				// mem_address = kernel
	li s1, 1					// unsigned block_id
	# li a7, BIOS_SDREAD

	li t0, os_size_loc
	lh s2, 0(t0)				// s2 = os_size			

sd_read_loop:
	li t2, 64
	bltu s2, t2, final_read_sd_read 	// branch if s2 <= 64

	li a7, BIOS_SDREAD
	mv a0, s0					// unsigned mem_address
	li a1, 64					// a1: num_of_block
	mv a2, s1					// unsigned block_id
	jal bios_func_entry

	li t3, sd_read_size
	add s0, s0, t3				// mem_address += sd_read_size
	addi s2, s2, -64			// os_size -= 64
	addi s1, s1, 64				// sd block_id += 64
	j sd_read_loop

final_read_sd_read:
	li a7, BIOS_SDREAD
	mv a0, s0					// unsigned mem_address
	mv a1, s2					// unsigned num_of_blocks
	mv a2, s1					// unsigned block_id
	jal bios_func_entry



	# li a7, BIOS_SDREAD
	# li a0, kernel				// unsigned mem_address
	# li t0, os_size_loc
	# lh a1, 0(t0)			// unsigned num_of_blocks
	# li a2, 1					// unsigned block_id
	# jal bios_func_entry		

	jal kernel					// jump to kernel entry 	

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS

	/************************************************************/
secondary:
	// lock slave_hart_lock
	la t0, slave_hart_lock
	li t1, 0x1
	sh t1, 0(t0)

	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
  	/* Mask all interrupts */
  	csrw sie, zero
  	csrw sip, zero
	// let stvec pointer to kernel_main
	la t0, kernel
	csrw stvec, t0
	// enable software interrupt
	li t0, 0x2	 
	csrs sie, t0		# SIE_SSIE		
	csrs sstatus, t0	# SSTATUS_SIE		

wait_for_wakeup:
	wfi
	j wait_for_wakeup
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's aihuachun's bootloader...\n\r"

