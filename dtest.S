
test:     file format elf64-littleriscv


Disassembly of section .text:

0000000050000000 <_ftext>:
    50000000:	4281                	li	t0,0
    50000002:	4305                	li	t1,1
    50000004:	03200393          	li	t2,50

0000000050000008 <loop>:
    50000008:	929a                	add	t0,t0,t1
    5000000a:	0305                	addi	t1,t1,1
    5000000c:	fe63dee3          	bge	t2,t1,50000008 <loop>
    50000010:	00000e17          	auipc	t3,0x0
    50000014:	00ee0e13          	addi	t3,t3,14 # 5000001e <__DATA_BEGIN__>
    50000018:	005e3023          	sd	t0,0(t3)

000000005000001c <end>:
    5000001c:	a001                	j	5000001c <end>
