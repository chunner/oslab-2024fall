.section .data
addr:	.dword 0x0

.section .text
.global main
main:
	li t0,0
	li t1,1
	li t2,50

loop:
	add t0, t0, t1
	addi t1, t1, 1
	ble t1, t2, loop

	la t3, addr
	sd t0, 0(t3)

// TODO: [p0-task1] Add your code here!

end:
	j end
