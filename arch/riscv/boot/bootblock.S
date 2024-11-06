#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000  

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

// sd card read byte per time
.equ sd_read_size, 0x8000  // 512 * 64

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
	li a0, kernel				// a0: mem_address = kernel
	li a2, 1					// unsigned block_id
	li a7, BIOS_SDREAD

	li t0, os_size_loc
	lh t1, 0(t0)				// t1 = os_size			

sd_read_loop:
	li t2, 64
	bltu t1, t2, final_read_sd_read 	// branch if t1: os_size <= 64

	li a1, 64					// a1: num_of_block
	jal bios_func_entry
	li t3, sd_read_size
	add a0, a0, t3				// mem_address += sd_read_size
	addi t1, t1, -64			// os_size -= 64
	addi a2, a2, 64				// sd block_id += 64
	j sd_read_loop

final_read_sd_read:
	mv a1, t1
	jal bios_func_entry



	li a7, BIOS_SDREAD
	li a0, kernel				// unsigned mem_address
	li t0, os_size_loc
	lh a1, 0(t0)			// unsigned num_of_blocks
	li a2, 1					// unsigned block_id
	jal bios_func_entry		

	jal kernel					// jump to kernel entry 	

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS

	/************************************************************/
secondary:
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

