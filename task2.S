.section .data 
start_sp: .dword 0x0
# isprime: .asciz "%d is prime\n"
# notprime: .asciz "%d is not prime\n"
.section .text
.global main
.global judge



main:
// TODO: [p0-task1] Add your code here!
	la sp, start_sp 
	addi sp, sp, 100
	#ld sp, 0(t0)

	li t0, 1
	li t1, 200

loop:
	addi sp, sp, -16
	sd   t0, 8(sp)
	sd   t1, 0(sp)
	call judge
	ld  t0, 8(sp)
	ld  t1, 0(sp)
	addi sp, sp, 16

	addi t0, t0, 1		# next number
	ble t0, t1, loop

end:
	j end

# t0 is parameter
.type judge , @function
judge:
	li t1, 2
	bge t0, t1, check
	j not_prime                 # t1<2, is not prime

check:
	bge t1, t0, is_prime  # t1 >= t0
	rem t2, t0, t1   # t2 = t0 % t1
	beq t2, zero, not_prime
	
	addi t1, t1, 1
	j check



is_prime:
	li a0, 1          # a0 is return value
	ret

not_prime: 			  # a0 is return value
	li a0, 0
	ret



