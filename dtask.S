
task2:     file format elf64-littleriscv


Disassembly of section .text:

0000000050000000 <_ftext>:
    50000000:	00000117          	auipc	sp,0x0
    50000004:	04a10113          	addi	sp,sp,74 # 5000004a <__DATA_BEGIN__>
    50000008:	06410113          	addi	sp,sp,100
    5000000c:	4285                	li	t0,1
    5000000e:	0c800313          	li	t1,200

0000000050000012 <loop>:
    50000012:	1141                	addi	sp,sp,-16
    50000014:	e416                	sd	t0,8(sp)
    50000016:	e01a                	sd	t1,0(sp)
    50000018:	012000ef          	jal	ra,5000002a <judge>
    5000001c:	62a2                	ld	t0,8(sp)
    5000001e:	6302                	ld	t1,0(sp)
    50000020:	0141                	addi	sp,sp,16
    50000022:	0285                	addi	t0,t0,1
    50000024:	fe5357e3          	bge	t1,t0,50000012 <loop>

0000000050000028 <end>:
    50000028:	a001                	j	50000028 <end>

000000005000002a <judge>:
    5000002a:	4309                	li	t1,2
    5000002c:	0062d363          	bge	t0,t1,50000032 <check>
    50000030:	a819                	j	50000046 <not_prime>

0000000050000032 <check>:
    50000032:	00535863          	bge	t1,t0,50000042 <is_prime>
    50000036:	0262e3b3          	rem	t2,t0,t1
    5000003a:	00038663          	beqz	t2,50000046 <not_prime>
    5000003e:	0305                	addi	t1,t1,1
    50000040:	bfcd                	j	50000032 <check>

0000000050000042 <is_prime>:
    50000042:	4505                	li	a0,1
    50000044:	8082                	ret

0000000050000046 <not_prime>:
    50000046:	4501                	li	a0,0
    50000048:	8082                	ret
