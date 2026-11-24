	.file	"matrix_multiplication.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_zmmul1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	matrix_multiply
	.type	matrix_multiply, @function
matrix_multiply:
	mul	a5,a3,a3
	addi	sp,sp,-48
	sw	s0,44(sp)
	sw	s5,24(sp)
	sw	s6,20(sp)
	mv	s0,a3
	mv	s6,a0
	mv	s5,a2
	beq	a5,zero,.L2
	slli	a5,a5,2
	add	a4,a2,a5
	mv	a5,a2
.L3:
	sw	zero,0(a5)
	addi	a5,a5,4
	bne	a5,a4,.L3
.L2:
	beq	s0,zero,.L1
	sw	s7,16(sp)
	addi	s7,s0,-3
	sw	s1,40(sp)
	srli	s7,s7,1
	slli	s1,s0,2
	neg	a2,s0
	sw	s2,36(sp)
	sw	s3,32(sp)
	sw	s4,28(sp)
	sw	s8,12(sp)
	sw	s9,8(sp)
	addi	s7,s7,1
	add	t3,s5,s1
	slli	a2,a2,3
	mv	s4,s6
	mv	a0,s5
	li	s3,0
	li	s2,0
	li	s8,2
.L5:
	li	a3,0
	bleu	s0,s8,.L13
	slli	a3,s7,1
	mv	t4,s1
	mv	t0,s4
	li	t1,0
	li	t2,0
.L6:
	lw	t6,0(t0)
	lw	t5,4(t0)
	sub	a6,a1,t1
	mv	a4,a0
.L10:
	add	a7,a6,t1
	lw	a5,0(a6)
	add	a7,a7,t4
	lw	a7,0(a7)
	mul	a5,t6,a5
	lw	s9,0(a4)
	addi	a4,a4,4
	addi	a6,a6,4
	mul	a7,t5,a7
	add	a5,a5,s9
	add	a5,a5,a7
	sw	a5,-4(a4)
	bne	a4,t3,.L10
	addi	t2,t2,2
	addi	t0,t0,8
	add	t1,t1,a2
	sub	t4,t4,a2
	bne	t2,a3,.L6
.L13:
	mul	t4,s0,a3
	slli	t5,s3,2
	add	t6,s6,t5
	add	t5,s5,t5
.L8:
	slli	a5,a3,2
	add	a5,t6,a5
	slli	a7,t4,2
	lw	t1,0(a5)
	add	a7,a1,a7
	mv	a5,t5
.L7:
	lw	a6,0(a7)
	lw	a4,0(a5)
	addi	a5,a5,4
	mul	a6,t1,a6
	addi	a7,a7,4
	add	a4,a4,a6
	sw	a4,-4(a5)
	bne	a5,t3,.L7
	addi	a3,a3,1
	add	t4,t4,s0
	bgtu	s0,a3,.L8
	addi	s2,s2,1
	add	a0,a0,s1
	add	s3,s3,s0
	add	t3,t3,s1
	add	s4,s4,s1
	bne	s0,s2,.L5
	lw	s1,40(sp)
	lw	s2,36(sp)
	lw	s3,32(sp)
	lw	s4,28(sp)
	lw	s7,16(sp)
	lw	s8,12(sp)
	lw	s9,8(sp)
.L1:
	lw	s0,44(sp)
	lw	s5,24(sp)
	lw	s6,20(sp)
	addi	sp,sp,48
	jr	ra
	.size	matrix_multiply, .-matrix_multiply
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits
