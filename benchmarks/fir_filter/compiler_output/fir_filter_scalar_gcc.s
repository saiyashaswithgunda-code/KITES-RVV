	.file	"fir_filter.c"
	.option nopic
	.attribute arch, "rv32i2p1_m2p0_zmmul1p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.globl	fir_filter
	.type	fir_filter, @function
fir_filter:
	beq	a3,zero,.L1
	beq	a4,zero,.L11
	mv	t5,a3
	bleu	a3,a4,.L4
	mv	t5,a4
.L4:
	mv	t2,a2
	mv	t0,a0
	li	t6,0
	addi	t4,a0,-4
.L6:
	addi	t6,t6,1
	mv	a7,a1
	mv	a5,t0
	li	t1,0
.L5:
	lw	a6,0(a7)
	lw	t3,0(a5)
	addi	a5,a5,-4
	addi	a7,a7,4
	mul	a6,a6,t3
	add	t1,t1,a6
	bne	t4,a5,.L5
	sw	t1,0(t2)
	addi	t0,t0,4
	addi	t2,t2,4
	bgtu	t5,t6,.L6
	bleu	a3,a4,.L20
.L3:
	slli	a5,t5,2
	add	t1,a2,a5
	add	t3,a0,a5
.L10:
	addi	t5,t5,1
	bne	a4,zero,.L21
	sw	zero,0(t1)
	addi	t3,t3,4
	addi	t1,t1,4
	bgtu	a3,t5,.L10
.L1:
	ret
.L11:
	li	t5,0
	j	.L3
.L21:
	slli	a4,a4,2
	add	a7,a1,a4
.L9:
	mv	a2,t3
	mv	a5,a1
	li	a0,0
.L8:
	lw	a4,0(a2)
	lw	a6,0(a5)
	addi	a5,a5,4
	addi	a2,a2,-4
	mul	a4,a4,a6
	add	a0,a0,a4
	bne	a5,a7,.L8
	sw	a0,0(t1)
	bleu	a3,t5,.L1
	addi	t1,t1,4
	addi	t3,t3,4
	addi	t5,t5,1
	j	.L9
.L20:
	ret
	.size	fir_filter, .-fir_filter
	.ident	"GCC: (Arch Linux Repositories) 16.2.0"
	.section	.note.GNU-stack,"",@progbits
