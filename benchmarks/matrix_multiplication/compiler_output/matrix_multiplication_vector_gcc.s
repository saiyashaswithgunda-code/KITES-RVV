	.file	"matrix_multiplication.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_f2p2_d2p2_v1p0_zicsr2p0_zmmul1p0_zve32f1p0_zve32x1p0_zve64d1p0_zve64f1p0_zve64x1p0_zvl128b1p0_zvl32b1p0_zvl64b1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	matrix_multiply
	.type	matrix_multiply, @function
matrix_multiply:
	mul	a4,a3,a3
	addi	sp,sp,-32
	sw	s0,28(sp)
	sw	s2,20(sp)
	mv	s0,a3
	mv	s2,a1
	beq	a4,zero,.L2
	vsetvli	a5,zero,e32,m1,ta,ma
	vmv.v.i	v1,0
	mv	a3,a2
.L3:
	vsetvli	a5,a4,e8,mf4,ta,ma
	vse32.v	v1,0(a3)
	sub	a4,a4,a5
	slli	a5,a5,2
	add	a3,a3,a5
	bne	a4,zero,.L3
.L2:
	beq	s0,zero,.L27
	sw	s3,16(sp)
	addi	s3,s0,-3
	srli	s3,s3,1
	sw	s1,24(sp)
	sw	s4,12(sp)
	mv	t5,a2
	mv	a1,a0
	addi	s3,s3,1
	slli	t2,s0,2
	li	s1,0
	li	s4,2
.L5:
	mv	a7,s2
	slli	a3,s3,1
	mv	t4,a1
	li	t6,0
	bleu	s0,s4,.L31
.L6:
	lw	a4,0(t4)
	lw	a5,4(t4)
	vsetvli	a2,zero,e32,m1,ta,ma
	vmv.v.x	v4,a4
	vmv.v.x	v3,a5
	add	t0,a7,t2
	mv	t3,t0
	mv	t1,t5
	mv	a6,s0
	mv	a0,t5
	mv	a2,t5
.L10:
	vsetvli	a5,a6,e32,m1,ta,ma
	vle32.v	v2,0(t1)
	vle32.v	v1,0(a7)
	slli	a4,a5,2
	sub	a6,a6,a5
	add	t1,t1,a4
	add	a7,a7,a4
	vmadd.vv	v1,v4,v2
	vse32.v	v1,0(a2)
	add	a2,a2,a4
	vle32.v	v2,0(t3)
	add	t3,t3,a4
	vmacc.vv	v1,v2,v3
	vse32.v	v1,0(a0)
	add	a0,a0,a4
	bne	a6,zero,.L10
	addi	t6,t6,2
	addi	t4,t4,8
	add	a7,t2,t0
	bne	a3,t6,.L6
.L13:
	mul	t1,t2,a3
	add	t1,s2,t1
.L8:
	slli	a5,a3,2
	add	a5,a1,a5
	lw	a5,0(a5)
	vsetvli	a4,zero,e32,m1,ta,ma
	mv	a7,t5
	vmv.v.x	v3,a5
	mv	a6,t1
	mv	a2,s0
	mv	a4,t5
.L7:
	vsetvli	a5,a2,e32,m1,ta,ma
	vle32.v	v1,0(a7)
	vle32.v	v2,0(a6)
	slli	a0,a5,2
	sub	a2,a2,a5
	add	a7,a7,a0
	add	a6,a6,a0
	vmacc.vv	v1,v3,v2
	vse32.v	v1,0(a4)
	add	a4,a4,a0
	bne	a2,zero,.L7
	addi	a3,a3,1
	add	t1,t1,t2
	bgtu	s0,a3,.L8
	addi	s1,s1,1
	add	t5,t5,t2
	add	a1,a1,t2
	bne	s0,s1,.L5
	lw	s1,24(sp)
	lw	s3,16(sp)
	lw	s4,12(sp)
.L27:
	lw	s0,28(sp)
	lw	s2,20(sp)
	addi	sp,sp,32
	jr	ra
.L31:
	li	a3,0
	j	.L13
	.size	matrix_multiply, .-matrix_multiply
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits
